# Profiling with xperf

Xperf is part of the Microsoft Windows Performance Toolkit, and has
functionality similar to that of Shark, oprofile, and (for some things)
dtrace/Instruments. For stack walking, Windows Vista or higher is
required; I haven't tested it at all on XP.

This page applies to xperf version **4.8.7701 or newer**. To see your
xperf version, either run '`xperf`' on a command line with no
arguments, or start '`xperfview`' and look at Help -\> About
Performance Analyzer. (Note that it's not the first version number in
the About window; that's the Windows version.)

If you have an older version, you will experience bugs, especially
around symbol loading for local builds.

## Installation

For all versions, the tools are part of the latest [Windows 7 SDK (SDK
Version
7.1)](http://www.microsoft.com/downloads/details.aspx?FamilyID=6b6c21d2-2006-4afa-9702-529fa782d63b&displaylang=en "http://www.microsoft.com/downloads/details.aspx?FamilyID=6b6c21d2-2006-4afa-9702-529fa782d63b&displaylang=en"){.external}.
Use the web installer to install at least the \"Win32 Development
Tools\". Once the SDK installs, execute either `wpt_x86.msi` or
`wpt_x64.msi` in the `Redist/Windows Performance Toolkit `folder of the
SDK's install location (typically Program Files/Microsoft
SDKs/Windows/v7.1/Redist/Windows Performance Toolkit) to actually
install the Windows Performance Toolkit tools.

It might already be installed by the Windows SDK. Check if C:\\Program
Files\\Microsoft Windows Performance Toolkit already exists.

For 64-bit Windows 7 or Vista, you'll need to do a registry tweak and
then restart to enable stack walking:\
\
`REG ADD "HKLM\System\CurrentControlSet\Control\Session Manager\Memory Management" -v DisablePagingExecutive -d 0x1 -t REG_DWORD -f`

## Symbol Server Setup

With the latest versions of the Windows Performance Toolkit, you can
modify the symbol path directly from within the program via the Trace
menu. Just make sure you set the symbol paths before enabling \"Load
Symbols\" and before opening a summary view. You can also modify the
`_NT_SYMBOL_PATH` and `_NT_SYMCACHE_PATH` environment variables to make
these changes permanent.

The standard symbol path that includes both Mozilla's and Microsoft's
symbol server configuration is as follows:

`_NT_SYMCACHE_PATH: C:\symbols  _NT_SYMBOL_PATH: srv*c:\symbols*http://msdl.microsoft.com/download/symbols;srv*c:\symbols*http://symbols.mozilla.org/firefox/`

To add symbols **from your own builds**, add
`C:\path\to\objdir\dist\bin` to `_NT_SYMBOL_PATH`. As with all Windows
paths, the symbol path uses semicolons (`;`) as separators.

Make sure you select the Trace -\> Load Symbols menu option in the
Windows Performance Analyzer (xperfview).

There seems to be a bug in xperf and symbols; it is very sensitive to
when the symbol path is edited. If you change it within the program,
you'll have to close all summary tables and reopen them for it to pick
up the new symbol path data.

You'll have to agree to a EULA for the Microsoft symbols \-- if you're
not prompted for this, then something isn't configured right in your
symbol path. (Again, make sure that the directories exist; if they
don't, it's a silent error.)

## Quick Start

All these tools will live, by default, in C:\\Program Files\\Microsoft
Windows Performance Toolkit. Either run these commands from there, or
add the directory to your path. You will need to use an elevated command
prompt to start or stop profiling.

Start recording data:

`xperf -on latency -stackwalk profile`

\"Latency\" is a special provider name that turns on a few predefined
kernel providers; run \"xperf -providers k\" to view a full list of
providers and groups. You can combine providers, e.g., \"xperf -on
DiagEasy+FILE_IO\". \"-stackwalk profile\" tells xperf to capture a
stack for each PROFILE event; you could also do \"-stackwalk
profile+file_io\" to capture a stack on each cpu profile tick and each
file io completion event.

Stop:

`xperf -d out.etl`

View:

`xperfview out.etl`

The MSDN
\"[Quickstart](http://msdn.microsoft.com/en-us/library/ff190971%28v=VS.85%29.aspx){.external}\"
page goes over this in more detail, and also has good explanations of
how to use xperfview. I'm not going to repeat it here, because I'd be
using essentially the same screenshots, so go look there.

The 'stack' view will give results similar to shark.

## Heap Profiling

xperf has good tools for heap allocation profiling, but they have one
major limitation: you can't build with jemalloc and get heap events
generated. The stock windows CRT allocator is horrible about
fragmentation, and causes memory usage to rise drastically even if only
a small fraction of that memory is in use. However, even despite this,
it's a useful way to track allocations/deallocations.

### Capturing Heap Data

The \"-heap\" option is used to set up heap tracing. Firefox generates
lots of events, so you may want to play with the
BufferSize/MinBuffers/MaxBuffers options as well to ensure that you
don't get dropped events. Also, when recording the stack, I've found
that a heap trace is often missing module information (I believe this is
a bug in xperf). It's possible to get around that by doing a
simultaneous capture of non-heap data.

To start a trace session, launching a new Firefox instance:

`xperf -on base  xperf -start heapsession -heap -PidNewProcess "./firefox.exe -P test -no-remote" -stackwalk HeapAlloc+HeapRealloc -BufferSize 512 -MinBuffers 128 -MaxBuffers 512`

To stop a session and merge the resulting files:

`xperf -stop heapsession -d heap.etl  xperf -d main.etl  xperf -merge main.etl heap.etl result.etl`

\"result.etl\" will contain your merged data; you can delete main.etl
and heap.etl. Note that it's possible to capture even more data for the
non-heap profile; for example, you might want to be able to correlate
heap events with performance data, so you can do
\"`xperf -on base -stackwalk profile`\".

In the viewer, when summary data is viewed for heap events (Heap
Allocations Outstanding, etc. all lead to the same summary graphs), 3
types of allocations are listed \-- AIFI, AIFO, AOFI. This is shorthand
for \"Allocated Inside, Freed Inside\", \"Allocated Inside, Freed
Outside\", \"Allocated Outside, Freed Inside\". These refer to the time
range that was selected for the summary graph; for example, something
that's in the AOFI category was allocated before the start of the
selected time range, but the free event happened inside.

## Tips

-   In the summary views, the yellow bar can be dragged left and right
    to change the grouping \-- for example, drag it to the left of the
    Module column to have grouping happen only by process (stuff that's
    to the left), so that you get symbols in order of weight, regardless
    of what module they're in.
-   Dragging the columns around will change grouping in various ways;
    experiment to get the data that you're looking for. Also experiment
    with turning columns on and off; removing a column will allow data
    to be aggregated without considering that column's contributions.
-   Disabling all but one core will make the numbers add up to 100%.
    This can be done by running 'msconfig' and going to Advance
    Options from the \"Boot\" tab.

## Building Firefox

To get good data from a Firefox build, it is important to build with the
following options in your mozconfig:

`export CFLAGS="-Oy-"  export CXXFLAGS="-Oy-"`

This disables frame-pointer optimization which lets xperf do a much
better job unwinding the stack. Traces can be captured fine without this
option (for example, from nightlies), but the stack information will not
be useful.

`ac_add_options --enable-debug-symbols`

This gives us symbols.

## For More Information

Microsoft's [documentation for xperf](http://msdn.microsoft.com/en-us/library/ff191077.aspx "http://msdn.microsoft.com/en-us/library/ff191077.aspx")
is pretty good; there is a lot of depth to this tool, and you should
look there for more details.
