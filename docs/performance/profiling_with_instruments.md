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

## Kernel stacks

Under "File" -> "Recording Options" you can enable "Record Kernel Callstacks".
To get full symbols and not just the exported ones, you'll to install the matching
[Kernel Debug Kit](https://developer.apple.com/download/all/?q=Kernel%20Debug%20Kit).
Make sure you install the one whose macOS version exactly matches your version,
including the identifier at the end (e.g. "12.3.1 (21E258)").

### Allow Instruments to find kernel symbols

Installing the KDK is often not enough for Instruments to find the symbols.
Instruments uses Spotlight to find the dSYMs with the matching UUID, so you
need to put the dSYM in a place where Spotlight will index it.

First, check the UUID of your macOS installation's kernel. To do so, run the
following:

```
% dwarfdump --uuid /System/Library/Kernels/kernel.release.t6000
UUID: C342869F-FFB9-3CCE-A5A3-EA711C1E87F6 (arm64e) /System/Library/Kernels/kernel.release.t6000
```

Then, find the corresponding dSYM file in the KDK that you installed, and
run `mdls` on it. For example:

```
% mdls /Library/Developer/KDKs/KDK_12.3.1_21E258.kdk/System/Library/Kernels/kernel.release.t6000.dSYM
```

(Make sure you use the `.release` variant, not the `.development` variant
or any of the others.)

If the output from `mdls` contains the string `com_apple_xcode_dsym_uuids`
and the UUID matches, you're done.

Otherwise, try copying the `kernel.release.t6000.dSYM` bundle to your home
directory, and then run `mdls` on the copied file. For example:

```
% cp -r /Library/Developer/KDKs/KDK_12.3.1_21E258.kdk/System/Library/Kernels/kernel.release.t6000.dSYM ~/
% mdls ~/kernel.release.t6000.dSYM
_kMDItemDisplayNameWithExtensions      = "kernel.release.t6000.dSYM"
com_apple_xcode_dsym_paths             = (
    "Contents/Resources/DWARF/kernel.release.t6000"
)
com_apple_xcode_dsym_uuids             = (
    "C342869F-FFB9-3CCE-A5A3-EA711C1E87F6"
)
kMDItemContentCreationDate             = 2022-03-21 15:25:57 +0000
[...]
```

Now Instruments should be able to pick up the kernel symbols.

## Misc

The `DTPerformanceSession` api can be used to control profiling from
applications like the old CHUD API we use in Shark builds. [Bug
667036](https://bugzilla.mozilla.org/show_bug.cgi?id=667036 "https://bugzilla.mozilla.org/show_bug.cgi?id=667036")

System Trace might be useful.
