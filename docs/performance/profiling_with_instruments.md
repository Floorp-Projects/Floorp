# Profiling with Instruments

Instruments can be used for memory profiling and for statistical
profiling.

## Official Apple documentation

-   [Instruments User
    Guide](https://developer.apple.com/library/mac/documentation/DeveloperTools/Conceptual/InstrumentsUserGuide/)
-   [Instruments User
    Reference](https://developer.apple.com/library/mac/documentation/AnalysisTools/Reference/Instruments_User_Reference/)
-   [Instruments Help
    Articles](https://developer.apple.com/library/mac/recipes/Instruments_help_articles/)
-   [Instruments
    Help](https://developer.apple.com/library/mac/recipes/instruments_help-collection/)
-   [Performance
    Overview](https://developer.apple.com/library/mac/documentation/Performance/Conceptual/PerformanceOverview/)

### Basic Usage

-   Select \"Time Profiler\" from the \"Choose a profiling template
    for:\" dialog.
-   In the top left, next to the record and pause button, there will be
    a \"\[machine name\] \> All Processes\". Click \"All Processes\" and
    select \"firefox\" from the \"Running Applications\" section.
-   Click the record button (red circle in top left)
-   Wait for the amount of time that you want to profile
-   Click the stop button

## Command line tools

There is
[instruments](https://developer.apple.com/library/mac/documentation/Darwin/Reference/Manpages/man1/instruments.1.html)
and
[iprofiler](https://developer.apple.com/library/mac/documentation/Darwin/Reference/Manpages/man1/iprofiler.1.html).

How do we monitor performance counters (cache miss etc.)? Instruments
has a \"Counters\" instrument that can do this.

## Memory profiling

Instruments will record a call stack at each allocation point. The call
tree view can be quite helpful here. Switch from \"Statistics\". This
`malloc` profiling is done using the `malloc_logger` infrastructure
(similar to `MallocStackLogging`). Currently this means you need to
build with jemalloc disabled (`ac_add_options --disable-jemalloc`). You
also need the fix to [Bug
719427](https://bugzilla.mozilla.org/show_bug.cgi?id=719427 "https://bugzilla.mozilla.org/show_bug.cgi?id=719427")

The `DTPerformanceSession` api can be used to control profiling from
applications like the old CHUD API we use in Shark builds. [Bug
667036](https://bugzilla.mozilla.org/show_bug.cgi?id=667036 "https://bugzilla.mozilla.org/show_bug.cgi?id=667036")

System Trace might be useful.
