# Performance

This page explains how optimize the performance the Firefox code base

The [test documentation](../../testing/perfdocs/)
explains how to test for performance in Firefox. 
The [profiler documentation](../../tools/profiler/)
explains how to use the Gecko profiler. 

## General Performance references
* Tips on generating valid performance metrics by [benchmarking](benchmarking.md)
* [GPU Performance](GPU_performance.md) Tips for reducing impacts on browser performance in front-end code.
* [Automated Performance testing and Sheriffing](automated_performance_testing_and_sheriffing.md) Information on automated performance testing and sheriffing at Mozilla.
* [Performance best practices for Firefox front-end engineers](bestpractices.md) Tips for reducing impacts on browser performance in front-end code.
* [Reporting a performance problem](reporting_a_performance_problem.md) A user friendly guide to reporting a performance problem. A development environment is not required.
* [Scroll Linked Effects](scroll-linked_effects.md) Information on scroll-linked effects, their effect on performance, related tools, and possible mitigation techniques.

## Memory profiling and leak detection tools
* The [Developer Tools Memory panel](memory/memory.md) supports taking heap snapshots, diffing them, computing dominator trees to surface "heavy retainers", and recording allocation stacks.
* [About:memory](memory/about_colon_memory.md) about:memory is the easiest-to-use tool for measuring memory usage in Mozilla code, and is the best place to start. It also lets you do other memory-related operations like trigger GC and CC, dump GC & CC logs, and dump DMD reports. about:memory is built on top of Firefox's memory reporting infrastructure.
* [DMD](memory/DMD.md) is a tool that identifies shortcomings in about:memory's measurements, and can also do multiple kinds of general heap profiling.
* [AWSY](memory/awsy.md) (are we slim yet?) is a memory usage and regression tracker.
* [Bloatview](memory/bloatview.md) prints per-class statistics on allocations and refcounts, and provides gross numbers on the amount of memory being leaked broken down by class. It is used as part of Mozilla's continuous integration testing.
* [Refcount Tracing and Balancing](memory/refcount_tracing_and_balancing.md) are ways to track down leaks caused by incorrect uses of reference counting. They are slow and not particular easy to use, and thus most suitable for use by expert developers.
* [GC and CC Logs](memory/GC_and_CC_logs.md)
* [Leak Gauge](memory/leak_gauge.md) can be generated and analyzed to in various ways. In particular, they can help you understand why a particular object is being kept alive.
* [LogAlloc](https://searchfox.org/mozilla-central/source/memory/replace/logalloc/README) is a tool that dumps a log of memory allocations in Gecko. That log can then be replayed against Firefox's default memory allocator independently or through another replace-malloc library, allowing the testing of other allocators under the exact same workload.
* [See also the documentation on Leak-hunting strategies and tips.](leak_hunting_strategies_and_tips.md) 
