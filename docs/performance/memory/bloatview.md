# Bloatview

BloatView is a tool that shows information about cumulative memory usage
and leaks. If it finds leaks, you can use [refcount tracing and balancing](refcount_tracing_and_balancing.md)
to discover the root cause.

## How to build with BloatView

Build with `--enable-debug` or `--enable-logrefcnt`.

## How to run with BloatView

The are two environment variables that can be used.

    XPCOM_MEM_BLOAT_LOG

If set, this causes a *bloat log* to be printed on program exit, and
each time `nsTraceRefcnt::DumpStatistics` is called. This log contains
data on leaks and bloat (a.k.a. usage).

    XPCOM_MEM_LEAK_LOG

This is similar to `XPCOM_MEM_BLOAT_LOG`, but restricts the log to only
show data on leaks.

You can set these environment variables to any of the following values.

-   **1** - log to stdout.
-   **2** - log to stderr.
-   ***filename*** - write log to a file.

## Reading individual bloat logs

Full BloatView output contains per-class statistics on allocations and
refcounts, and provides gross numbers on the amount of memory being
leaked broken down by class. Here\'s a sample of the BloatView output.

    == BloatView: ALL (cumulative) LEAK AND BLOAT STATISTICS, tab process 1862
        |<----------------Class--------------->|<-----Bytes------>|<----Objects---->|
        |                                      | Per-Inst   Leaked|   Total      Rem|
      0 |TOTAL                                 |       17     2484|253953338       38|
     17 |AsyncTransactionTrackersHolder        |       40       40|   10594        1|
     78 |CompositorChild                       |      472      472|       1        1|
     79 |CondVar                               |       24       48|    3086        2|
    279 |MessagePump                           |        8        8|      30        1|
    285 |Mutex                                 |       20       60|   89987        3|
    302 |PCompositorChild                      |      412      412|       1        1|
    308 |PImageBridgeChild                     |      416      416|       1        1|

The first line tells you the pid of the leaking process, along with the
type of process.

Here's how you interpret the columns.

-   The first, numerical column [is theindex](https://searchfox.org/mozilla-central/source/xpcom/base/nsTraceRefcnt.cpp#365)
    of the leaking class.
-   **Class** - The name of the class in question (truncated to 20
    characters).
-   **Bytes Per-Inst** - The number of bytes returned if you were to
    write `sizeof(Class)`. Note that this number does not reflect any
    memory held onto by the class, such as internal buffers, etc. (E.g.
    for `nsString` you\'ll see the size of the header struct, not the
    size of the string contents!)
-   **Bytes Leaked** - The number of bytes per instance times the number
    of objects leaked: (Bytes Per-Inst) x (Objects Rem). Use this number
    to look for the worst offenders. (Should be zero!)
-   **Objects Total** - The total count of objects allocated of a given
    class.
-   **Objects Rem** - The number of objects allocated of a given class
    that weren't deleted. (Should be zero!)

Interesting things to look for:

-   **Are your classes in the list?** - Look! If they aren't, then
    you're not using the `NS_IMPL_ADDREF` and `NS_IMPL_RELEASE` (or
    `NS_IMPL_ISUPPORTS` which calls them) for xpcom objects, or
    `MOZ_COUNT_CTOR` and `MOZ_COUNT_DTOR` for non-xpcom objects. Not
    having your classes in the list is *not* ok. That means no one is
    looking at them, and we won't be able to tell if someone introduces
    a leak. (See
    [How_to_instrument_your_objects_for_BloatView](bloatview.html#how-to-instrument-your-objects-for-bloatview)
    for how to fix this.)
-   **The Bytes Leaked for your classes should be zero!** - Need I say
    more? If it isn't, you should use the other tools to fix it.
-   **The number of objects remaining might not be equal to the total
    number of objects.** This could indicate a hand-written Release
    method (that doesn't use the `NS_LOG_RELEASE` macro from
    nsTraceRefcnt.h), or perhaps you\'re just not freeing any of the
    instances you've allocated. These sorts of leaks are easy to fix.
-   **The total number of objects might be 1.** This might indicate a
    global variable or service. Usually this will have a large number of
    refcounts.

If you find leaks, you can use [refcount tracing and balancing](refcount_tracing_and_balancing.md)
to discover the root cause.

## Combining and sorting bloat logs

You can view one or more bloat logs in your browser by running the
following program.

    perl tools/bloatview/bloattable.pl *log1* *log2* \... *logn* \>
    *htmlfile*

This will produce an HTML file that contains a table similar to the
following (but with added JavaScript so you can sort the data by
column).

    Byte Bloats

       ---------- ---------------- --------------------------
       Name       File             Date
       blank      `blank.txt`      Tue Aug 29 14:17:40 2000
       mozilla    `mozilla.txt`    Tue Aug 29 14:18:42 2000
       yahoo      `yahoo.txt`      Tue Aug 29 14:19:32 2000
       netscape   `netscape.txt`   Tue Aug 29 14:20:14 2000
       ---------- ---------------- --------------------------

The numbers do not include malloc d data such as string contents.

Click on a column heading to sort by that column. Click on a class name
to see details for that class.

       -------------------- --------------- ----------------- --------- --------- ---------- ---------- ------------------------------- --------- -------- ---------- ---------
       Class Name           Instance Size   Bytes allocated                                             Bytes allocated but not freed                                 
                                            blank             mozilla   yahoo     netscape   Total      blank                           mozilla   yahoo    netscape   Total
       TOTAL                                                                                            1754408                         432556    179828   404184     2770976
       nsStr                20              6261600           3781900   1120920   1791340    12955760   222760                          48760     13280    76160      360960
       nsHashKey            8               610568            1842400   2457872   1134592    6045432    32000                           536       568      1216       34320
       nsTextTransformer    548             8220              469088    1414936   1532756    3425000    0                               0         0        0          0
       nsStyleContextData   736             259808            325312    489440    338560     1413120    141312                          220800    -11040   94944      446016
       nsLineLayout         1100            2200              225500    402600    562100     1192400    0                               0         0        0          0
       nsLocalFile          424             558832            19928     1696      1272       581728     72080                           1272      424      -424       73352
       -------------------- --------------- ----------------- --------- --------- ---------- ---------- ------------------------------- --------- -------- ---------- ---------

The first set of columns, **Bytes allocated**, shows the amount of
memory allocated for the first log file (`blank.txt`), the difference
between the first log file and the second (`mozilla.txt`), the
difference between the second log file and the third (`yahoo.txt`), the
difference between the third log file and the fourth (`netscape.txt`),
and the total amount of memory allocated in the fourth log file. These
columns provide an idea of how hard the memory allocator has to work,
but they do not indicate the size of the working set.

The second set of columns, **Bytes allocated but not freed**, shows the
net memory gain or loss by subtracting the amount of memory freed from
the amount allocated.

The **Show Objects** and **Show References** buttons show the same
statistics but counting objects or `AddRef`\'d references rather than
bytes.

## Comparing Bloat Logs

You can also compare any two bloat logs (either those produced when the
program shuts down, or written to the bloatlogs directory) by running
the following program.

    `perl tools/bloatview/bloatdiff.pl` \<previous-log\> \<current-log\>

This will give you output of the form:

     Bloat/Leak Delta Report
     Current file:  dist/win32_D.OBJ/bin/bloatlogs/all-1999-10-22-133450.txt
     Previous file: dist/win32_D.OBJ/bin/bloatlogs/all-1999-10-16-010302.txt
     --------------------------------------------------------------------------
     CLASS                     LEAKS       delta      BLOAT       delta
     --------------------------------------------------------------------------
     TOTAL                   6113530       2.79%   67064808       9.18%
     StyleContextImpl         265440      81.19%     283584     -26.99%
     CToken                   236500      17.32%     306676      20.64%
     nsStr                    217760      14.94%    5817060       7.63%
     nsXULAttribute           113048     -70.92%     113568     -71.16%
     LiteralImpl               53280      26.62%      75840      19.40%
     nsXULElement              51648       0.00%      51648       0.00%
     nsProfile                 51224       0.00%      51224       0.00%
     nsFrame                   47568     -26.15%      48096     -50.49%
     CSSDeclarationImpl        42984       0.67%      43488       0.67%

This "delta report" shows the leak offenders, sorted from most leaks
to fewest. The delta numbers show the percentage change between runs for
the amount of leaks and amount of bloat (negative numbers are better!).
The bloat number is a metric determined by multiplying the total number
of objects allocated of a given class by the class size. Note that
although this isn\'t necessarily the amount of memory consumed at any
given time, it does give an indication of how much memory we\'re
consuming. The more memory in general, the worse the performance and
footprint. The percentage 99999.99% will show up indicating an
"infinite" amount of leakage. This happens when something that didn't
leak before is now leaking.

## BloatView and continuous integration

BloatView runs on debug builds for many of the test suites Mozilla has
running under continuous integration. If a new leak occurs, it will
trigger a test job failure.

BloatView's output file can also show you where the leaked objects are
allocated. To do so, the `XPCOM_MEM_LOG_CLASSES` environment variable
should be set to the name of the class from the BloatView table:

    XPCOM_MEM_LOG_CLASSES=MyClass mach mochitest [options]

Multiple class names can be specified by setting `XPCOM_MEM_LOG_CLASSES`
to a comma-separated list of names:

    XPCOM_MEM_LOG_CLASSES=MyClass,MyOtherClass,DeliberatelyLeakedClass mach mochitest [options]

Test harness scripts typically accept a `--setenv` option for specifying
environment variables, which may be more convenient in some cases:

    mach mochitest --setenv=XPCOM_MEM_LOG_CLASSES=MyClass [options]

For getting allocation stacks in automation, you can add the appropriate
`--setenv` options to the test configurations for the platforms you\'re
interested in. Those configurations are located in
`testing/mozharness/configs/`. The most likely configs you\'ll want to
modify are listed below:

-   Linux: `unittests/linux_unittest.py`
-   Mac: `unittests/mac_unittest.py`
-   Windows: `unittests/win_unittest.py`
-   Android: `android/androidarm_4_3.py`

## How to instrument your objects for BloatView

First, if your object is an xpcom object and you use the
`NS_IMPL_ADDREF` and `NS_IMPL_RELEASE` (or a variation thereof) macro to
implement your `AddRef` and `Release` methods, then there is nothing you
need do. By default, those macros support refcnt logging directly.

If your object is not an xpcom object then some manual editing is in
order. The following sample code shows what must be done:

    MyType::MyType()
    {
      MOZ_COUNT_CTOR(MyType);
      ...
    }

    MyType::~MyType()
    {
      MOZ_COUNT_DTOR(MyType);
      ...
    }
