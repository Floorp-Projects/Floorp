# powermetrics

`powermetrics` is a Mac-only command-line utility that provides many
high-quality power-related measurements. It is most useful for getting
CPU, GPU and wakeup measurements in a precise and easily scriptable
fashion (unlike [Activity Monitor and
top](activity_monitor_and_top.md))
especially in combination with
[rapl](tools_power_rapl.md) via the
`mach power` command. This document describes the version of
`powermetrics` that comes with Mac OS 10.10. The one that comes with
10.9 is less powerful.

**Note**: The [power profiling
overview](power_profiling_overview.md) is
worth reading at this point if you haven\'t already. It may make parts
of this document easier to understand.

## Quick start

`powermetrics` provides a vast number of measurements. The following
command encompasses the most useful ones:

sudo powermetrics --samplers tasks --show-process-coalition --show-process-gpu -n 1 -i 5000

-   `--samplers tasks` tells it to just do per-process measurements.
-   `--show-process-coalition`` `tells it to group *coalitions* of
    related processes, e.g. the Firefox parent process and child
    processes.
-   `--show-process-gpu` tells it to show per-process GPU measurements.
-   `-n 1` tells it to take one sample and then stop.
-   `-i 5000` tells it to use a sample length of 5 seconds (5000 ms).
    Change this number to get shorter or longer samples.

The following is example output from such an invocation:

    *** Sampled system activity (Fri Sep  4 17:15:14 2015 +1000) (5009.63ms elapsed) ***

    *** Running tasks ***

    Name                               ID     CPU ms/s  User%  Deadlines (<2 ms, 2-5 ms)  Wakeups (Intr, Pkg idle)  GPU ms/s
    com.apple.Terminal                 293    447.66                                      274.83  120.35            221.74
      firefox                          84627  77.59     55.55  15.37   2.59               91.42   42.12             204.47
      plugin-container                 84628  377.22    37.18  43.91   18.56              178.65  75.85             17.29
      Terminal                         694    9.86      79.94  0.00    0.00               4.39    2.20              0.00
      powermetrics                     84694  1.21      31.53  0.00    0.00               0.20    0.20              0.00
    com.google.Chrome                  489    233.83                                      48.10   25.95             0.00
      Google Chrome Helper             84688  181.57    92.81  0.00    0.00               23.95   12.77             0.00
      Google Chrome                    84681  57.26     76.07  4.39    0.00               23.75   12.97             0.00
      Google Chrome Helper             84685  0.13      48.08  0.00    0.00               0.40    0.20              0.00
    kernel_coalition                   1      128.64                                      780.19  330.52            0.00
      kernel_task                      0      109.97    0.00   0.20    0.00               779.47  330.35            0.00
      launchd                          1      18.88     2.44   0.00    0.00               0.40    0.20              0.00
    com.apple.Safari                   488    90.60                                       108.58  56.48             26.65
      com.apple.WebKit.WebContent      84679  64.21     84.69  0.00    0.00               104.19  54.89             26.66
      com.apple.WebKit.Networking      84678  26.89     58.89  0.40    0.00               1.60    0.00              0.00
      Safari                           84676  1.56      55.74  0.00    0.00               2.59    1.40              0.00
      com.apple.Safari.SearchHelper    84690  0.15      49.49  0.00    0.00               0.20    0.20              0.00
    org.mozilla.firefox                482    76.56                                       124.34  63.47             0.00
      firefox                          84496  76.70     89.18  10.58   5.59               124.55  63.48             0.00

This sample was taken while the following programs were running:

-   Firefox Beta (single process, invoked from the Mac OS dock, shown in
    the `org.mozilla.firefox` coalition.)
-   Firefox Nightly (multi-process, invoked from the command line, shown
    in the `com.apple.Terminal` coalition.)
-   Google Chrome.
-   Safari.

The grouping of parent and child processes (in coalitions) is obvious.
The meaning of the columns is as follows.

-   **Name**: Coalition/process name. Process names within coalitions
    are indented.
-   **ID**: Coalition/process ID number.
-   **CPU ms/s**: CPU time used by the coalition/process, per second,
    during the sample period. The sum of the process values typically
    exceeds the coalition value slightly, for unknown reasons.
-   **User%**: Percentage of that CPU time spent in user space (as
    opposed to kernel mode.)
-   **Deadlines (\<2 ms, 2-5 ms)**: These two columns count how many
    \"short\" timers woke up threads in the process, per second, during
    the sample period. High frequency timers, which typically have short
    time-to-deadlines, can cause high power consumption and should be
    avoided if possible.
-   **Wakeups (Intr, Pkg idle)**: These two columns count how many
    wakeups occurred, per second, during the sample period. The first
    column counts interrupt-level wakeups that resulted in a thread
    being dispatched in the process. The second column counts \"package
    idle exit\" wakeups, which wake up the entire package as opposed to
    just a single core; such wakeups are particularly expensive, and
    this count is a subset of the first column\'s count.
-   **GPU ms/s**: GPU time used by the coalition/process, per second,
    during the sample period.

Other things to note.

-   Smaller is better --- i.e. results in lower power consumption ---
    for all of these measurements.
-   There is some overlap between the two \"Deadlines\" columns and the
    two \"Wakeups\" columns. For example, firing a single sub-2ms
    deadline can also cause a package idle exit wakeup.
-   Many of these measurements are also obtainable by passing the
    `TASK_POWER_INFO` flag and a `task_power_info` struct to the
    `task_info` function.
-   By default, the coalitions/processes are sorted by a composite value
    computed from several factors, though this can be changed via
    command-line options.

## Other measurements

`powermetrics` can also report measurements of backlight usage, network
activity, disk activity, interrupt distribution, device power states,
C-state residency, P-state residency, quality of service classes, and
thermal pressure. These are less likely to be useful for profiling
Firefox, however. Run with the `--show-all` to see all of these at once,
but note that you\'ll need a very wide window to see all the data.

Also note that `powermetrics -h` is a better guide to the the
command-line options than `man powermetrics`.

## mach power

You can use the `mach power` command to run `powermetrics` in
combination with `rapl` in a way that gives the most useful summary
measurements for each of Firefox, Chrome and Safari. The following is
sample output.

        total W = _pkg_ (cores + _gpu_ + other) + _ram_ W
    #01 17.14 W = 14.98 ( 5.50 +  1.19 +  8.29) +  2.16 W

    1 sample taken over a period of 30.000 seconds

    Name                               ID     CPU ms/s  User%  Deadlines (<2 ms, 2-5 ms)  Wakeups (Intr, Pkg idle)  GPU ms/s
    com.google.Chrome                  500    439.64                                      585.35  218.62            19.17
      Google Chrome Helper             67319  284.75    83.03  296.67  0.00               454.05  172.74            0.00
      Google Chrome Helper             67304  55.23     64.83  0.03    0.00               9.43    4.33              19.17
      Google Chrome                    67301  63.77     68.09  29.46   0.13               76.11   22.26             0.00
      Google Chrome Helper             67320  38.30     66.70  17.83   0.00               45.78   19.29             0.00
    com.apple.WindowServer             68     102.58                                      112.36  43.15             80.52
      WindowServer                     141    103.03    58.19  60.48   6.40               112.36  43.15             80.53
    com.apple.Safari                   499    267.19                                      110.53  46.05             1.69
      com.apple.WebKit.WebContent      67372  190.15    79.34  2.02    0.14               129.28  53.79             2.33
      com.apple.WebKit.Networking      67292  65.23     52.74  0.07    0.00               4.33    1.40              0.00
      Safari                           67290  29.09     77.65  0.23    0.00               7.13    3.37              0.00
      com.apple.Safari.SearchHelper    67371  13.88     91.18  0.00    0.00               0.36    0.05              0.00
      com.apple.WebKit.WebContent      67297  0.81      56.84  0.10    0.00               2.20    1.30              0.00
      com.apple.WebKit.WebContent      67293  0.46      76.40  0.03    0.00               0.57    0.20              0.00
      com.apple.WebKit.WebContent      67295  0.24      67.72  0.00    0.00               0.90    0.37              0.00
      com.apple.WebKit.WebContent      67298  0.17      59.88  0.00    0.00               0.50    0.13              0.00
      com.apple.WebKit.WebContent      67296  0.07      43.51  0.00    0.00               0.10    0.03              0.00
    kernel_coalition                   1      111.76                                      724.80  213.09            0.12
      kernel_task                      0      107.06    0.00   5.86    0.00               724.46  212.99            0.12
    org.mozilla.firefox                498    92.17                                       212.69  75.67             1.81
      firefox                          63865  61.00     87.18  1.00    0.87               25.79   9.00              1.81
      plugin-container                 67269  31.49     72.46  1.80    0.00               186.90  66.68             0.00
      com.apple.WebKit.Plugin.64       67373  55.55     74.38  0.74    0.00               9.51    3.13              0.02
    com.apple.Terminal                 109    6.22                                        0.40    0.23              0.00
      Terminal                         208    6.25      92.99  0.00    0.00               0.33    0.20              0.00

The `rapl` output is first, then the `powermetrics` output. As well as
the browser processes, the `WindowServer` and kernel tasks are shown
because browsers often trigger significant load in them.

The default sample period is 30,000 milliseconds (30 seconds), but that
can be changed with the `-i` option.
