# tools/power/rapl

`tools/power/rapl` (or `rapl` for short) is a command-line utility in
the Mozilla tree that periodically reads and prints all available Intel
RAPL power estimates. These are machine-wide estimates, so if you want
to estimate the power consumption of a single program you should
minimize other activity on the machine while measuring.

**Note**: The [power profiling overview](power_profiling_overview.md) is
worth reading at this point if you haven't already. It may make parts
of this document easier to understand.

## Invocation

First, do a [standard build of Firefox](setup/index.html).

### Mac

On Mac, `rapl` can be run as follows.

``` {.brush: .bash}
$OBJDIR/dist/bin/rapl
```

### Linux

On Linux, `rapl` can be run as root, as follows.

    sudo $OBJDIR/dist/bin/rapl

Alternatively, it can be run without root privileges by setting the
contents of
[/proc/sys/kernel/perf_event_paranoid](http://unix.stackexchange.com/questions/14227/do-i-need-root-admin-permissions-to-run-userspace-perf-tool-perf-events-ar)
to 0. Note that if you do change this file, its contents may reset when
the machine is next rebooted.

You must be running Linux kernel version 3.14 or later for `rapl` to
work. Otherwise, it will fail with an error message explaining this
requirement.

### Windows

Unfortunately, `rapl` does not work on Windows, and porting it would be
difficult because Windows does not have APIs that allow easy access to
the relevant model-specific registers.

## Output

The following is 10 seconds of output from a default invocation of
`rapl`.

``` {.brush: .bash}
    total W = _pkg_ (cores + _gpu_ + other) + _ram_ W
#01  5.17 W =  1.78 ( 0.12 +  0.10 +  1.56) +  3.39 W
#02  9.43 W =  5.44 ( 1.44 +  1.20 +  2.80) +  3.98 W
#03 14.26 W = 10.21 ( 5.47 +  0.19 +  4.55) +  4.04 W
#04 10.02 W =  6.15 ( 2.62 +  0.43 +  3.10) +  3.86 W
#05 14.63 W = 10.43 ( 4.41 +  0.81 +  5.22) +  4.19 W
#06 11.16 W =  6.90 ( 1.91 +  1.68 +  3.31) +  4.26 W
#07  5.40 W =  1.97 ( 0.20 +  0.10 +  1.67) +  3.44 W
#08  5.17 W =  1.76 ( 0.07 +  0.08 +  1.60) +  3.41 W
#09  5.17 W =  1.76 ( 0.09 +  0.08 +  1.58) +  3.42 W
#10  8.13 W =  4.40 ( 1.55 +  0.11 +  2.74) +  3.73 W
```

Things to note include the following.

-   All measurements are in Watts.
-   The first line indicates the meaning of each column.
-   The underscores in `_pkg_`, `_gpu_` and `_ram_` are present so that
    each column's name has five characters.
-   The total power is the sum of the package power and the RAM power.
-   The package estimate is divided into three parts: cores, GPU, and
    \"other\". \"Other\" is computed as the package power minus the
    cores power and GPU power.
-   If the processor does not support GPU or RAM estimates then
    \"` n/a `\" will be printed in the relevant column instead of a
    number, and it will contribute zero to the total.

Once sampling is finished --- either because the user interrupted it, or
because the requested number of samples has been taken --- the following
summary data is shown:

``` {.brush: .bash}
10 samples taken over a period of 10.000 seconds

Distribution of 'total' values:
            mean =  8.85 W
         std dev =  3.50 W
  0th percentile =  5.17 W (min)
  5th percentile =  5.17 W
 25th percentile =  5.17 W
 50th percentile =  8.13 W
 75th percentile = 11.16 W
 95th percentile = 14.63 W
100th percentile = 14.63 W (max)
```

The distribution data is omitted if there was zero or one samples taken.

## Options

-   `-i --sample-interval`. The length of each sample in milliseconds.
    Defaults to 1000. A warning is given if you set it below 50 because
    that is likely to lead to inaccurate estimates.
-   `-n --sample-count`. The number of samples to take. The default is
    0, which is interpreted as \"unlimited\".

## Combining with `powermetrics`

On Mac, you can use the [mach power](powermetrics.html#mach-power) command
to run `rapl` in combination with `powermetrics` in a way that gives the
most useful summary measurements for each of Firefox, Chrome and Safari.
