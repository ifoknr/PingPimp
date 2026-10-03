#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdarg.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <sched.h>
#include <signal.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>

#define MODDIR "/data/adb/modules/PingPimp"
#define PKG_LIST_FILE "/data/system/packages.list"
#define LOG_FILE MODDIR "/log.txt"
#define LOG_MAX_SIZE (256 * 1024)
#define PID_FILE MODDIR "/daemon.pid"
#define IRQ_BACKUP_FILE MODDIR "/irq_backup.txt"
#define LOWAT_VALUE "16384"
#define LOWAT_DEFAULT "4294967295"

void trim(char *str) {
    char *p = str;
    int l = strlen(p);
    while(l > 0 && isspace(p[l - 1])) p[--l] = 0;
    while(*p && isspace(*p)) ++p, --l;
    memmove(str, p, l + 1);
}

int write_file(const char *path, const char *val) {
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    int ok = fputs(val, f) >= 0;
    if (fclose(f) != 0) ok = 0;
    return ok;
}

void log_msg(const char *fmt, ...) {
    struct stat st;
    const char *mode = (stat(LOG_FILE, &st) == 0 && st.st_size > LOG_MAX_SIZE) ? "w" : "a";
    FILE *f = fopen(LOG_FILE, mode);
    if (!f) return;

    char ts[32];
    time_t now = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(f, "[%s] ", ts);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

void exec_cmd(const char *cmd) {
    system(cmd);
}

int read_config(const char *filename, char *buffer, size_t size) {
    FILE *f = fopen(filename, "r");
    if (!f) return 0;
    if (fgets(buffer, size, f)) {
        trim(buffer);
        fclose(f);
        return 1;
    }
    fclose(f);
    return 0;
}

void get_uid(const char *pkg, char *uid_out) {
    FILE *f = fopen(PKG_LIST_FILE, "r");
    uid_out[0] = '\0';
    if (!f) return;
    
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char curr_pkg[256];
        char curr_uid[64];
        if (sscanf(line, "%255s %63s", curr_pkg, curr_uid) == 2) {
            if (strcmp(curr_pkg, pkg) == 0) {
                strcpy(uid_out, curr_uid);
                break;
            }
        }
    }
    fclose(f);
}

void apply_default() {
    write_file("/proc/sys/net/core/netdev_max_backlog", "4096");
    write_file("/proc/sys/net/core/rmem_default", "262144");
    write_file("/proc/sys/net/core/rmem_max", "524288");
    write_file("/proc/sys/net/core/wmem_default", "262144");
    write_file("/proc/sys/net/core/wmem_max", "524288");
    write_file("/proc/sys/net/ipv4/tcp_low_latency", "0");
    write_file("/proc/sys/net/ipv4/tcp_slow_start_after_idle", "1");
    write_file("/proc/sys/net/ipv4/tcp_syn_retries", "4");
    write_file("/proc/sys/net/ipv4/tcp_rmem", "4096 87380 1048576");
    write_file("/proc/sys/net/ipv4/tcp_wmem", "4096 65536 1048576");
    write_file("/proc/sys/net/ipv4/udp_rmem_min", "8192");
    write_file("/proc/sys/net/ipv4/udp_wmem_min", "8192");
    write_file("/proc/sys/net/ipv4/tcp_fastopen", "3");
    write_file("/proc/sys/net/ipv4/tcp_fin_timeout", "20");
    write_file("/proc/sys/net/ipv4/tcp_window_scaling", "1");
    write_file("/proc/sys/net/ipv4/tcp_timestamps", "1");
    write_file("/proc/sys/net/ipv4/tcp_no_metrics_save", "0");
    write_file("/proc/sys/net/ipv4/route/flush", "1");
    exec_cmd("cmd wifi force-low-latency-mode disabled 2>/dev/null");
}

void apply_game() {
    write_file("/proc/sys/net/core/netdev_max_backlog", "1000");
    write_file("/proc/sys/net/core/rmem_default", "131072");
    write_file("/proc/sys/net/core/rmem_max", "262144");
    write_file("/proc/sys/net/core/wmem_default", "131072");
    write_file("/proc/sys/net/core/wmem_max", "262144");
    write_file("/proc/sys/net/ipv4/tcp_low_latency", "1");
    write_file("/proc/sys/net/ipv4/tcp_slow_start_after_idle", "0");
    write_file("/proc/sys/net/ipv4/tcp_syn_retries", "2");
    write_file("/proc/sys/net/ipv4/tcp_rmem", "4096 65536 262144");
    write_file("/proc/sys/net/ipv4/tcp_wmem", "4096 65536 262144");
    write_file("/proc/sys/net/ipv4/udp_rmem_min", "32768");
    write_file("/proc/sys/net/ipv4/udp_wmem_min", "32768");
    write_file("/proc/sys/net/ipv4/tcp_fastopen", "3");
    write_file("/proc/sys/net/ipv4/tcp_fin_timeout", "10");
    write_file("/proc/sys/net/ipv4/tcp_window_scaling", "1");
    write_file("/proc/sys/net/ipv4/tcp_timestamps", "0");
    write_file("/proc/sys/net/ipv4/tcp_no_metrics_save", "1");
    write_file("/proc/sys/net/ipv4/route/flush", "1");
    exec_cmd("cmd wifi force-low-latency-mode enabled 2>/dev/null");
}

void apply_download() {
    write_file("/proc/sys/net/core/netdev_max_backlog", "16384");
    write_file("/proc/sys/net/core/rmem_default", "524288");
    write_file("/proc/sys/net/core/rmem_max", "4194304");
    write_file("/proc/sys/net/core/wmem_default", "524288");
    write_file("/proc/sys/net/core/wmem_max", "4194304");
    write_file("/proc/sys/net/ipv4/tcp_low_latency", "0");
    write_file("/proc/sys/net/ipv4/tcp_slow_start_after_idle", "0");
    write_file("/proc/sys/net/ipv4/tcp_syn_retries", "3");
    write_file("/proc/sys/net/ipv4/tcp_rmem", "4096 87380 4194304");
    write_file("/proc/sys/net/ipv4/tcp_wmem", "4096 65536 4194304");
    write_file("/proc/sys/net/ipv4/udp_rmem_min", "8192");
    write_file("/proc/sys/net/ipv4/udp_wmem_min", "8192");
    write_file("/proc/sys/net/ipv4/tcp_fastopen", "3");
    write_file("/proc/sys/net/ipv4/tcp_fin_timeout", "30");
    write_file("/proc/sys/net/ipv4/tcp_window_scaling", "1");
    write_file("/proc/sys/net/ipv4/tcp_timestamps", "1");
    write_file("/proc/sys/net/ipv4/tcp_no_metrics_save", "0");
    write_file("/proc/sys/net/ipv4/route/flush", "1");
    exec_cmd("cmd wifi force-low-latency-mode disabled 2>/dev/null");
}

void apply_streaming() {
    write_file("/proc/sys/net/core/netdev_max_backlog", "8192");
    write_file("/proc/sys/net/core/rmem_default", "262144");
    write_file("/proc/sys/net/core/rmem_max", "2097152");
    write_file("/proc/sys/net/core/wmem_default", "262144");
    write_file("/proc/sys/net/core/wmem_max", "2097152");
    write_file("/proc/sys/net/ipv4/tcp_low_latency", "0");
    write_file("/proc/sys/net/ipv4/tcp_slow_start_after_idle", "0");
    write_file("/proc/sys/net/ipv4/tcp_syn_retries", "3");
    write_file("/proc/sys/net/ipv4/tcp_rmem", "4096 87380 2097152");
    write_file("/proc/sys/net/ipv4/tcp_wmem", "4096 65536 2097152");
    write_file("/proc/sys/net/ipv4/udp_rmem_min", "16384");
    write_file("/proc/sys/net/ipv4/udp_wmem_min", "16384");
    write_file("/proc/sys/net/ipv4/tcp_fastopen", "3");
    write_file("/proc/sys/net/ipv4/tcp_fin_timeout", "20");
    write_file("/proc/sys/net/ipv4/tcp_window_scaling", "1");
    write_file("/proc/sys/net/ipv4/tcp_timestamps", "1");
    write_file("/proc/sys/net/ipv4/tcp_no_metrics_save", "0");
    write_file("/proc/sys/net/ipv4/route/flush", "1");
    exec_cmd("cmd wifi force-low-latency-mode disabled 2>/dev/null");
}

void apply_social() {
    write_file("/proc/sys/net/core/netdev_max_backlog", "4096");
    write_file("/proc/sys/net/core/rmem_default", "131072");
    write_file("/proc/sys/net/core/rmem_max", "524288");
    write_file("/proc/sys/net/core/wmem_default", "131072");
    write_file("/proc/sys/net/core/wmem_max", "524288");
    write_file("/proc/sys/net/ipv4/tcp_low_latency", "1");
    write_file("/proc/sys/net/ipv4/tcp_slow_start_after_idle", "0");
    write_file("/proc/sys/net/ipv4/tcp_syn_retries", "3");
    write_file("/proc/sys/net/ipv4/tcp_rmem", "4096 65536 1048576");
    write_file("/proc/sys/net/ipv4/tcp_wmem", "4096 65536 1048576");
    write_file("/proc/sys/net/ipv4/udp_rmem_min", "8192");
    write_file("/proc/sys/net/ipv4/udp_wmem_min", "8192");
    write_file("/proc/sys/net/ipv4/tcp_fastopen", "3");
    write_file("/proc/sys/net/ipv4/tcp_fin_timeout", "15");
    write_file("/proc/sys/net/ipv4/tcp_window_scaling", "1");
    write_file("/proc/sys/net/ipv4/tcp_timestamps", "1");
    write_file("/proc/sys/net/ipv4/tcp_no_metrics_save", "0");
    write_file("/proc/sys/net/ipv4/route/flush", "1");
    exec_cmd("cmd wifi force-low-latency-mode enabled 2>/dev/null");
}

void apply_outdoor() {
    write_file("/proc/sys/net/core/netdev_max_backlog", "8192");
    write_file("/proc/sys/net/core/rmem_default", "262144");
    write_file("/proc/sys/net/core/rmem_max", "1048576");
    write_file("/proc/sys/net/core/wmem_default", "262144");
    write_file("/proc/sys/net/core/wmem_max", "1048576");
    write_file("/proc/sys/net/ipv4/tcp_low_latency", "0");
    write_file("/proc/sys/net/ipv4/tcp_slow_start_after_idle", "1");
    write_file("/proc/sys/net/ipv4/tcp_syn_retries", "6");
    write_file("/proc/sys/net/ipv4/tcp_rmem", "4096 87380 1048576");
    write_file("/proc/sys/net/ipv4/tcp_wmem", "4096 65536 1048576");
    write_file("/proc/sys/net/ipv4/udp_rmem_min", "16384");
    write_file("/proc/sys/net/ipv4/udp_wmem_min", "16384");
    write_file("/proc/sys/net/ipv4/tcp_fastopen", "3");
    write_file("/proc/sys/net/ipv4/tcp_fin_timeout", "30");
    write_file("/proc/sys/net/ipv4/tcp_window_scaling", "1");
    write_file("/proc/sys/net/ipv4/tcp_timestamps", "1");
    write_file("/proc/sys/net/ipv4/tcp_no_metrics_save", "0");
    write_file("/proc/sys/net/ipv4/route/flush", "1");
    exec_cmd("cmd wifi force-low-latency-mode disabled 2>/dev/null");
}

void apply_hw_tweaks() {
    exec_cmd("HEX_MASK=$(printf '%x' $(( (1 << 4) - 1 ))); "
             "for IFACE in /sys/class/net/wlan* /sys/class/net/rmnet*; do "
             "if [ -d \"$IFACE\" ]; then "
             "for Q in \"$IFACE\"/queues/*/rps_cpus; do "
             "[ -f \"$Q\" ] && echo \"$HEX_MASK\" > \"$Q\" 2>/dev/null; "
             "done; fi; done");
             
    exec_cmd("CONF=\"/data/misc/wifi/wpa_supplicant.conf\"; "
             "if [ -f \"$CONF\" ]; then "
             "grep -q \"p2p_disabled=1\" \"$CONF\" || echo \"p2p_disabled=1\" >> \"$CONF\"; "
             "grep -q \"ap_scan=1\" \"$CONF\" || echo \"ap_scan=1\" >> \"$CONF\"; "
             "fi; "
             "iw wlan0 set power_save off 2>/dev/null");
             
    exec_cmd("for iface in lo wlan0 rmnet_data0 rmnet_ipa0 dummy0 rmnet0 rndis0; do "
             "if [ -d \"/sys/class/net/$iface\" ]; then "
             "case \"$iface\" in "
             "lo|dummy0) Q=100 ;; "
             "wlan0) Q=3000 ;; "
             "rmnet*) Q=2500 ;; "
             "*) Q=1000 ;; "
             "esac; "
             "ifconfig \"$iface\" txqueuelen $Q 2>/dev/null; "
             "ip link set \"$iface\" txqueuelen $Q 2>/dev/null; "
             "fi; done");

    exec_cmd("stop tcpdump 2>/dev/null; stop vendor.tcpdump 2>/dev/null; "
             "stop cnss_diag 2>/dev/null; stop vendor.cnss_diag 2>/dev/null; "
             "if [ -d /data/vendor/wlan_logs ]; then "
             "rm -rf /data/vendor/wlan_logs/*; chmod 000 /data/vendor/wlan_logs; fi");
}

void init_tc() {
    const char *tc_script = 
        "IFACE=$(ip route get 1.1.1.1 2>/dev/null | awk '{print $5}' | head -n1); "
        "[ -z \"$IFACE\" ] && exit 0; "
        "tc qdisc del dev $IFACE root 2>/dev/null; "
        "tc qdisc add dev $IFACE root handle 1: prio bands 3 2>/dev/null; "
        "tc qdisc add dev $IFACE parent 1:1 handle 10: fq_codel 2>/dev/null || tc qdisc add dev $IFACE parent 1:1 handle 10: pfifo_fast 2>/dev/null; "
        "tc filter add dev $IFACE parent 1: protocol ip prio 1 u32 match ip tos 0xb8 0xfc flowid 1:1 2>/dev/null; "
        "tc filter add dev $IFACE parent 1: protocol ip prio 2 handle 0x40000000/0x40000000 fw flowid 1:1 2>/dev/null; "
        "tc filter add dev $IFACE parent 1: protocol ipv6 prio 2 handle 0x40000000/0x40000000 fw flowid 1:1 2>/dev/null;";
    exec_cmd(tc_script);
}

int check_dscp_support() {
    int status = system("iptables -t mangle -A OUTPUT -m owner --uid-owner 99999 -j DSCP --set-dscp 46 2>/dev/null");
    if (status == 0) {
        system("iptables -t mangle -D OUTPUT -m owner --uid-owner 99999 -j DSCP --set-dscp 46 2>/dev/null");
        return 1;
    }
    return 0;
}

void apply_iptables_list(const char *filename, int mode) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s", MODDIR, filename);
    FILE *f = fopen(filepath, "r");
    if (!f) return;

    int use_dscp = 0;
    if (mode == 1) {
        use_dscp = check_dscp_support();
    }

    char line[4096];
    if (fgets(line, sizeof(line), f)) {
        char *token = strtok(line, ",");
        while (token != NULL) {
            char pkg[256];
            strcpy(pkg, token);
            trim(pkg);

            if (strlen(pkg) > 0) {
                char uid[64];
                get_uid(pkg, uid);

                if (strlen(uid) > 0) {
                    char cmd[512];
                    if (mode == 1) {
                        if (use_dscp) {
                            snprintf(cmd, sizeof(cmd), "iptables -t mangle -I OUTPUT -m owner --uid-owner %s -j DSCP --set-dscp 46 2>/dev/null", uid);
                            exec_cmd(cmd);
                            snprintf(cmd, sizeof(cmd), "ip6tables -t mangle -I OUTPUT -m owner --uid-owner %s -j DSCP --set-dscp 46 2>/dev/null", uid);
                            exec_cmd(cmd);
                        } else {
                            snprintf(cmd, sizeof(cmd), "iptables -t mangle -I OUTPUT -m owner --uid-owner %s -j MARK --set-mark 0x40000000/0x40000000 2>/dev/null", uid);
                            exec_cmd(cmd);
                            snprintf(cmd, sizeof(cmd), "ip6tables -t mangle -I OUTPUT -m owner --uid-owner %s -j MARK --set-mark 0x40000000/0x40000000 2>/dev/null", uid);
                            exec_cmd(cmd);
                        }
                    } else { // Isolate Mode
                        snprintf(cmd, sizeof(cmd), "iptables -I OUTPUT -m owner --uid-owner %s -j REJECT 2>/dev/null", uid);
                        exec_cmd(cmd);
                        snprintf(cmd, sizeof(cmd), "ip6tables -I OUTPUT -m owner --uid-owner %s -j REJECT 2>/dev/null", uid);
                        exec_cmd(cmd);
                    }
                }
            }
            token = strtok(NULL, ",");
        }
    }
    fclose(f);
}


// === PRESET BY NAME ===
void apply_preset_name(const char *name) {
    if (strcmp(name, "game") == 0) apply_game();
    else if (strcmp(name, "download") == 0) apply_download();
    else if (strcmp(name, "streaming") == 0) apply_streaming();
    else if (strstr(name, "social") != NULL) apply_social();
    else if (strcmp(name, "browsing") == 0) apply_social();
    else if (strcmp(name, "outdoor") == 0) apply_outdoor();
    else apply_default();
}

void read_saved_preset(char *out, size_t size) {
    if (!read_config(MODDIR "/preset.txt", out, size) || strlen(out) == 0) {
        snprintf(out, size, "default");
    }
    for (int i = 0; out[i]; i++) out[i] = tolower((unsigned char)out[i]);
}

int config_is_on(const char *file) {
    char buffer[16];
    return read_config(file, buffer, sizeof(buffer)) && strcmp(buffer, "1") == 0;
}

// === NIC OFFLOADING (GRO/GSO/TSO via ethtool ioctl) ===
int ethtool_set(int sock, const char *iface, unsigned int cmd, unsigned int value) {
    struct ethtool_value ev = { .cmd = cmd, .data = value };
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", iface);
    ifr.ifr_data = (char *)&ev;
    return ioctl(sock, SIOCETHTOOL, &ifr) == 0;
}

int set_nic_offload(int enable) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return 0;

    DIR *dir = opendir("/sys/class/net");
    if (!dir) {
        close(sock);
        return 0;
    }

    int changed = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        const char *iface = ent->d_name;
        if (iface[0] == '.' || strcmp(iface, "lo") == 0) continue;

        int ok = 0;
        ok |= ethtool_set(sock, iface, ETHTOOL_SGRO, enable);
        ok |= ethtool_set(sock, iface, ETHTOOL_SGSO, enable);
        ok |= ethtool_set(sock, iface, ETHTOOL_STSO, enable);
        if (ok) {
            changed++;
            log_msg("NIC offload %s on %s", enable ? "restored" : "disabled", iface);
        }
    }
    closedir(dir);
    close(sock);
    return changed > 0;
}

// === HARDWARE IRQ AFFINITY ===
static const char *net_irq_keywords[] = {
    "wlan", "wifi", "rmnet", "ipa", "cnss", "mhi", "wcn", "ath", "bcmdhd",
    "dhdpcie", "ccmni", "ccci", "wlbt", "mt76", NULL
};

int is_net_irq(const char *line) {
    char lower[512];
    size_t i;
    for (i = 0; line[i] && i < sizeof(lower) - 1; i++) lower[i] = tolower((unsigned char)line[i]);
    lower[i] = '\0';
    for (int k = 0; net_irq_keywords[k]; k++) {
        if (strstr(lower, net_irq_keywords[k])) return 1;
    }
    return 0;
}

// Builds a CPU list (e.g. "4,5,6,7") of the cores with the highest max frequency
// above the slowest cluster. Falls back to all cores on symmetric CPUs.
void get_perf_cpus(char *out, size_t size) {
    long ncpu = sysconf(_SC_NPROCESSORS_CONF);
    if (ncpu <= 0 || ncpu > 64) ncpu = 8;

    long freqs[64];
    long min_freq = -1;
    for (int i = 0; i < ncpu; i++) {
        char path[128], buf[32];
        snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", i);
        freqs[i] = read_config(path, buf, sizeof(buf)) ? atol(buf) : 0;
        if (freqs[i] > 0 && (min_freq < 0 || freqs[i] < min_freq)) min_freq = freqs[i];
    }

    out[0] = '\0';
    size_t len = 0;
    for (int i = 0; i < ncpu; i++) {
        if (freqs[i] > min_freq) {
            len += snprintf(out + len, size - len, "%s%d", len ? "," : "", i);
            if (len >= size) break;
        }
    }
    if (len == 0) snprintf(out, size, "0-%ld", ncpu - 1);
}

int set_irq_affinity(int enable) {
    if (!enable) {
        long ncpu = sysconf(_SC_NPROCESSORS_CONF);
        char all_cpus[32];
        snprintf(all_cpus, sizeof(all_cpus), "0-%ld", ncpu > 0 ? ncpu - 1 : 7);

        FILE *b = fopen(IRQ_BACKUP_FILE, "r");
        int restored = 0;
        if (b) {
            int irq;
            char cpus[256];
            while (fscanf(b, "%d %255s", &irq, cpus) == 2) {
                char path[64];
                snprintf(path, sizeof(path), "/proc/irq/%d/smp_affinity_list", irq);
                if (write_file(path, cpus) || write_file(path, all_cpus)) restored++;
            }
            fclose(b);
            unlink(IRQ_BACKUP_FILE);
        }
        log_msg("IRQ affinity restored for %d IRQs", restored);
        return 1;
    }

    char perf_cpus[256];
    get_perf_cpus(perf_cpus, sizeof(perf_cpus));

    FILE *f = fopen("/proc/interrupts", "r");
    if (!f) return 0;

    // Keep the first backup so repeated enables don't overwrite the original values.
    int take_backup = access(IRQ_BACKUP_FILE, F_OK) != 0;
    FILE *b = take_backup ? fopen(IRQ_BACKUP_FILE, "w") : NULL;

    int pinned = 0;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (isspace((unsigned char)*p)) p++;
        if (!isdigit((unsigned char)*p)) continue;
        int irq = atoi(p);
        if (!is_net_irq(line)) continue;

        char path[64], orig[256];
        snprintf(path, sizeof(path), "/proc/irq/%d/smp_affinity_list", irq);
        if (!read_config(path, orig, sizeof(orig))) continue;

        if (write_file(path, perf_cpus)) {
            if (b) fprintf(b, "%d %s\n", irq, orig);
            pinned++;
        }
    }
    fclose(f);
    if (b) fclose(b);

    log_msg("IRQ affinity: pinned %d network IRQs to CPUs %s", pinned, perf_cpus);
    return pinned > 0;
}

// === TCP NOTSENT LOWAT ===
int set_lowat(int enable) {
    int ok = write_file("/proc/sys/net/ipv4/tcp_notsent_lowat", enable ? LOWAT_VALUE : LOWAT_DEFAULT);
    log_msg("tcp_notsent_lowat %s (%s)", enable ? LOWAT_VALUE : LOWAT_DEFAULT, ok ? "ok" : "failed");
    return ok;
}

// === KSOFTIRQD BOOST ===
int set_ksoftirqd_boost(int enable) {
    DIR *dir = opendir("/proc");
    if (!dir) return 0;

    struct sched_param sp;
    sp.sched_priority = enable ? 1 : 0;
    int policy = enable ? SCHED_FIFO : SCHED_OTHER;

    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (!isdigit((unsigned char)ent->d_name[0])) continue;

        char path[64], comm[64];
        snprintf(path, sizeof(path), "/proc/%s/comm", ent->d_name);
        if (!read_config(path, comm, sizeof(comm))) continue;
        if (strncmp(comm, "ksoftirqd/", 10) != 0) continue;

        if (sched_setscheduler(atoi(ent->d_name), policy, &sp) == 0) count++;
    }
    closedir(dir);

    log_msg("ksoftirqd %s for %d threads", enable ? "boosted (SCHED_FIFO 1)" : "restored", count);
    return count > 0;
}

// === SMART AUTO MODE ===
static const char *streaming_pkgs[] = {
    "com.google.android.youtube", "com.netflix.mediaclient", "com.amazon.avod.thirdpartyclient",
    "com.disney.disneyplus", "com.spotify.music", "tv.twitch.android.app",
    "com.google.android.apps.youtube.music", "com.shahid.android", NULL
};

static const char *social_pkgs[] = {
    "com.whatsapp", "org.telegram.messenger", "com.instagram.android", "com.zhiliaoapp.musically",
    "com.ss.android.ugc.trill", "com.twitter.android", "com.facebook.katana", "com.snapchat.android",
    "com.discord", NULL
};

static const char *download_pkgs[] = {
    "com.android.vending", "com.google.android.apps.docs", "com.dv.adm", "idm.internet.download.manager",
    NULL
};

int pkg_in_list(const char *pkg, const char **list) {
    for (int i = 0; list[i]; i++) {
        if (strcmp(pkg, list[i]) == 0) return 1;
    }
    return 0;
}

int pkg_in_file(const char *pkg, const char *filename) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", MODDIR, filename);
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    int found = 0;
    char line[4096];
    if (fgets(line, sizeof(line), f)) {
        char *token = strtok(line, ",");
        while (token && !found) {
            trim(token);
            if (strcmp(token, pkg) == 0) found = 1;
            token = strtok(NULL, ",");
        }
    }
    fclose(f);
    return found;
}

// Reads the package of the resumed (foreground) activity, e.g. "com.foo/.Main" -> "com.foo".
int get_foreground_pkg(char *out, size_t size) {
    out[0] = '\0';
    FILE *fp = popen("dumpsys activity activities 2>/dev/null | grep -m1 -E 'mResumedActivity|topResumedActivity'", "r");
    if (!fp) return 0;

    char line[512];
    if (fgets(line, sizeof(line), fp)) {
        char *slash = strchr(line, '/');
        if (slash) {
            char *start = slash;
            while (start > line && !isspace((unsigned char)start[-1])) start--;
            size_t len = slash - start;
            if (len > 0 && len < size) {
                memcpy(out, start, len);
                out[len] = '\0';
            }
        }
    }
    pclose(fp);
    return out[0] != '\0';
}

const char *auto_preset_for(const char *pkg, const char *fallback) {
    if (pkg_in_file(pkg, "boost_apps.txt")) return "game";
    if (pkg_in_list(pkg, streaming_pkgs)) return "streaming";
    if (pkg_in_list(pkg, social_pkgs)) return "social";
    if (pkg_in_list(pkg, download_pkgs)) return "download";
    return fallback;
}

// === BACKGROUND DAEMON ===
int daemon_running(void) {
    char buffer[32];
    if (!read_config(PID_FILE, buffer, sizeof(buffer))) return 0;
    pid_t pid = atoi(buffer);
    if (pid <= 0 || kill(pid, 0) != 0) return 0;

    char path[64], cmdline[256] = {0};
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    size_t n = fread(cmdline, 1, sizeof(cmdline) - 1, f);
    fclose(f);
    cmdline[n] = '\0';
    return strstr(cmdline, "PingPimp") != NULL;
}

void daemon_loop(void) {
    char last_iface[64] = {0};
    char curr_iface[64] = {0};
    char last_auto_preset[32] = {0};
    int tick = 0;

    log_msg("Daemon started (pid %d)", getpid());

    while (1) {
        FILE *fp = popen("ip route get 1.1.1.1 2>/dev/null | awk '{print $5}' | head -n1", "r");
        if (fp) {
            curr_iface[0] = '\0';
            if (fgets(curr_iface, sizeof(curr_iface), fp) != NULL) {
                trim(curr_iface);
                if (strlen(curr_iface) > 0 && strcmp(curr_iface, last_iface) != 0) {
                    log_msg("Active interface changed: %s -> %s", last_iface[0] ? last_iface : "none", curr_iface);
                    strcpy(last_iface, curr_iface);
                    init_tc();
                }
            }
            pclose(fp);
        }

        // Auto mode is checked every other tick (~6s) to keep dumpsys overhead low.
        if (tick++ % 2 == 0) {
            if (config_is_on(MODDIR "/auto_mode.txt")) {
                char saved[64], pkg[256];
                read_saved_preset(saved, sizeof(saved));
                if (get_foreground_pkg(pkg, sizeof(pkg))) {
                    const char *target = auto_preset_for(pkg, saved);
                    if (strcmp(target, last_auto_preset) != 0) {
                        apply_preset_name(target);
                        snprintf(last_auto_preset, sizeof(last_auto_preset), "%s", target);
                        log_msg("Auto mode: %s -> preset %s", pkg, target);
                    }
                }
            } else {
                last_auto_preset[0] = '\0';
            }
        }
        sleep(3);
    }
}

// Forks a detached daemon; the parent returns immediately so callers (WebUI exec) don't hang.
void start_daemon(void) {
    if (daemon_running()) return;

    pid_t pid = fork();
    if (pid < 0) {
        log_msg("Failed to fork daemon");
        return;
    }
    if (pid > 0) return;

    setsid();
    int fd = open("/dev/null", O_RDWR);
    if (fd >= 0) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) close(fd);
    }

    char pidbuf[16];
    snprintf(pidbuf, sizeof(pidbuf), "%d", getpid());
    write_file(PID_FILE, pidbuf);

    daemon_loop();
    _exit(EXIT_SUCCESS);
}

// === MODE DISPATCH ===
// Returns 0 on success, non-zero on failure or unknown mode.
int run_mode(const char *mode) {
    if (strcmp(mode, "--default") == 0) apply_default();
    else if (strcmp(mode, "--game") == 0) apply_game();
    else if (strcmp(mode, "--download") == 0) apply_download();
    else if (strcmp(mode, "--streaming") == 0) apply_streaming();
    else if (strcmp(mode, "--social") == 0 || strcmp(mode, "--browsing") == 0) apply_social();
    else if (strcmp(mode, "--outdoor") == 0) apply_outdoor();

    else if (strcmp(mode, "--state") == 0) {
        exec_cmd("cmd settings put global netstats_enabled 0");
        exec_cmd("cmd wifi set-scan-always-available disabled");
        exec_cmd("cmd wifi set-verbose-logging disabled -l 0");
    }
    else if (strcmp(mode, "--unstate") == 0) {
        exec_cmd("cmd settings delete global netstats_enabled");
        exec_cmd("cmd wifi set-scan-always-available enabled");
        exec_cmd("cmd wifi set-verbose-logging enabled -l 1");
    }

    else if (strcmp(mode, "--saver") == 0) {
        exec_cmd("cmd netpolicy set restrict-background true");
        exec_cmd("cmd settings put global ingress_rate_limit_bytes_per_second 1048576");
    }
    else if (strcmp(mode, "--unsaver") == 0) {
        exec_cmd("cmd netpolicy set restrict-background false");
        exec_cmd("cmd settings delete global ingress_rate_limit_bytes_per_second");
    }

    else if (strcmp(mode, "--disable") == 0) {
        exec_cmd("sysctl -w net.ipv6.conf.all.disable_ipv6=1 >/dev/null 2>&1");
        exec_cmd("sysctl -w net.ipv6.conf.default.disable_ipv6=1 >/dev/null 2>&1");
        exec_cmd("sysctl -w net.ipv6.conf.lo.disable_ipv6=1 >/dev/null 2>&1");
    }
    else if (strcmp(mode, "--enable") == 0) {
        exec_cmd("sysctl -w net.ipv6.conf.all.disable_ipv6=0 >/dev/null 2>&1");
        exec_cmd("sysctl -w net.ipv6.conf.default.disable_ipv6=0 >/dev/null 2>&1");
        exec_cmd("sysctl -w net.ipv6.conf.lo.disable_ipv6=0 >/dev/null 2>&1");
    }

    else if (strcmp(mode, "--nic-offload") == 0) return set_nic_offload(0) ? 0 : 1;
    else if (strcmp(mode, "--nic-on") == 0) return set_nic_offload(1) ? 0 : 1;
    else if (strcmp(mode, "--irq-affinity") == 0) return set_irq_affinity(1) ? 0 : 1;
    else if (strcmp(mode, "--unirq-affinity") == 0) return set_irq_affinity(0) ? 0 : 1;
    else if (strcmp(mode, "--lowat") == 0) return set_lowat(1) ? 0 : 1;
    else if (strcmp(mode, "--unlowat") == 0) return set_lowat(0) ? 0 : 1;
    else if (strcmp(mode, "--boost-ksoft") == 0) return set_ksoftirqd_boost(1) ? 0 : 1;
    else if (strcmp(mode, "--unboost-ksoft") == 0) return set_ksoftirqd_boost(0) ? 0 : 1;

    else if (strcmp(mode, "--auto-on") == 0) {
        log_msg("Smart Auto Mode enabled");
        start_daemon();
    }
    else if (strcmp(mode, "--auto-off") == 0) {
        char saved[64];
        read_saved_preset(saved, sizeof(saved));
        apply_preset_name(saved);
        log_msg("Smart Auto Mode disabled, restored preset %s", saved);
    }

    else if (strcmp(mode, "--hw-tweak") == 0) apply_hw_tweaks();
    else if (strcmp(mode, "--init-tc") == 0) init_tc();
    else return 1;

    return 0;
}

// Applies a saved on/off switch at boot; does nothing if the user never touched it.
void boot_apply_switch(const char *file, const char *flag_on, const char *flag_off) {
    char buffer[16];
    if (!read_config(file, buffer, sizeof(buffer))) return;
    run_mode(strcmp(buffer, "1") == 0 ? flag_on : flag_off);
}

void boot(void) {
    char buffer[128];
    log_msg("Boot: applying saved configuration");

    read_saved_preset(buffer, sizeof(buffer));
    apply_preset_name(buffer);
    log_msg("Boot: preset %s", buffer);

    boot_apply_switch(MODDIR "/ipv6_state.txt", "--disable", "--enable");
    boot_apply_switch(MODDIR "/state.txt", "--state", "--unstate");
    boot_apply_switch(MODDIR "/saver.txt", "--saver", "--unsaver");

    if (read_config(MODDIR "/tcp.txt", buffer, sizeof(buffer)) && strlen(buffer) > 0) {
        write_file("/proc/sys/net/ipv4/tcp_congestion_control", buffer);
        log_msg("Boot: TCP congestion control %s", buffer);
    }

    apply_iptables_list("isolate_apps.txt", 0);
    apply_iptables_list("boost_apps.txt", 1);
    apply_hw_tweaks();
    init_tc();

    // IRQ backup from the previous boot is stale; affinities reset on reboot.
    unlink(IRQ_BACKUP_FILE);
    if (config_is_on(MODDIR "/nic.txt")) set_nic_offload(0);
    if (config_is_on(MODDIR "/irq.txt")) set_irq_affinity(1);
    if (config_is_on(MODDIR "/lowat.txt")) set_lowat(1);
    if (config_is_on(MODDIR "/ksoft.txt")) set_ksoftirqd_boost(1);

    unlink(PID_FILE);
    start_daemon();
}

int main(int argc, char *argv[]) {
    if (argc < 2) return 1;

    if (strcmp(argv[1], "--boot") == 0) {
        boot();
        return 0;
    }
    return run_mode(argv[1]);
}
