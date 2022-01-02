# Perf

`perf` is a powerful system-wide instrumentation service that is part of
Linux. This article discusses how it can be relevant to power profiling.

**Note**: The [power profiling
overview](power_profiling_overview.md) is
worth reading at this point if you haven't already. It may make parts
of this document easier to understand.

## Energy estimates

`perf` can access the Intel RAPL energy estimates. The following example
shows how to invoke it for this purpose.

```
sudo perf stat -a -r 1 \
    -e "power/energy-pkg/" \
    -e "power/energy-cores/" \
    -e "power/energy-gpu/" \
    -e "power/energy-ram/" \
    <command>
```

The `-a` is necessary; it means \"all cores\", and without it all the
measurements will be zero. The `-r 1` means `<command>` is executed
once; higher values can be used to get variations.

The output will look like the following.

```
Performance counter stats for 'system wide':

   51.58 Joules power/energy-pkg/     [100.00%]
   14.80 Joules power/energy-cores/   [100.00%]
    9.93 Joules power/energy-gpu/     [100.00%]
   27.38 Joules power/energy-ram/     [100.00%]

5.003049064 seconds time elapsed
```

It's not clear from the output, but the following relationship holds.

``` 
energy-pkg >= energy-cores + energy-gpu
```

The measurement is in Joules, which is usually less useful than Watts.

For these reasons
[rapl](tools_power_rapl.md) is usually a
better tool for measuring power consumption on Linux.

## Wakeups {#Wakeups}

`perf` can also be used to do [high-context profiling of
wakeups](http://robertovitillo.com/2014/02/04/idle-wakeups-are-evil/).
