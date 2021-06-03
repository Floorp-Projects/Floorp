# dtrace

`dtrace` is a powerful Mac OS X kernel instrumentation system that can
be used to profile wakeups. This article provides a light introduction
to it.

:::
**Note**: The [power profiling
overview](/en-US/docs/Mozilla/Performance/Power_profiling_overview) is
worth reading at this point if you haven't already. It may make parts
of this document easier to understand.
:::

## Invocation

`dtrace` must be invoked as the super-user. A good starting command for
profiling wakeups is the following.

``` 
sudo dtrace -n 'mach_kernel::wakeup { @[ustack()] = count(); }' -p $FIREFOX_PID > $OUTPUT_FILE
```

Let's break that down further.

-   The` -n` option combined with the `mach_kernel::wakeup` selects a
    *probe point*. `mach_kernel` is the *module name* and `wakeup` is
    the *probe name*. You can see a complete list of probes by running
    `sudo dtrace -l`.
-   The code between the braces is run when the probe point is hit. The
    above code counts unique stack traces when wakeups occur; `ustack`
    is short for \"user stack\", i.e. the stack of the userspace program
    executing.

Run that command for a few seconds and then hit [Ctrl]{.kbd} + [C]{.kbd}
to interrupt it. `dtrace` will then print to the output file a number of
stack traces, along with a wakeup count for each one. The ordering of
the stack traces can be non-obvious, so look at them carefully.

Sometimes the stack trace has less information than one would like.
It's unclear how to improve upon this.

## See also

dtrace is *very* powerful, and you can learn more about it by consulting
the following resources:

-   [The DTrace one-liner
    tutorial](https://wiki.freebsd.org/DTrace/Tutorial) from FreeBSD.
-   [DTrace tools](http://www.brendangregg.com/dtrace.html), by Brendan
    Gregg.
