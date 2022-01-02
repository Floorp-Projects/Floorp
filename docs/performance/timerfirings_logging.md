# TimerFirings Loggings

TimerFirings logging is a feature built into Gecko that prints a line of
data for every timer fired. This is useful because timer firings are a
major cause of wakeups, which can cause high power consumption.

**Note**: The [power profiling
overview](power_profiling_overview.md)
is worth reading at this point if you haven\'t already. It may make
parts of this document easier to understand.

## Invocation

TimerFirings logging uses Gecko\'s own logging mechanism, and so is able
to be used in any build. Set the following environment variable to
enable it.

    NSPR_LOG_MODULES=TimerFirings:4

## Output

Once enabled, TimerFirings will print one line of logging output per
timer fired. It\'s best to redirect this output to a file.

The following sample shows the basics of this output.

    -991946880[7f46c365ba00]: [6775]    fn timer (SLACK      100 ms): LayerActivityTracker
    -991946880[7f46c365ba00]: [6775]    fn timer (ONE_SHOT   250 ms): PresShell::sPaintSuppressionCallback
    -991946880[7f46c365ba00]: [6775]    fn timer (ONE_SHOT   160 ms): nsBrowserStatusFilter::TimeoutHandler
    -991946880[7f46c365ba00]: [6775] iface timer (ONE_SHOT   200 ms): 7f46964d7f80
    -1340643584[7f46c365ec00]: [6775]   obs timer (SLACK     1000 ms): 7f46a95a0200

Each line has the following information.

-   The first two values identify the thread. This is not especially
    useful.
-   The next value is the process ID (pid). This is useful in a
    multi-process scenario.
-   Next is the timer kind, one of `fn` (function), `iface` (interface)
    or `obs` (observer), which are the three kinds of timers that Gecko
    supports.
-   Then comes the function kind, one of `ONE_SHOT` (a single-use
    timer), `SLACK` or `PRECISE` (repeating timers of differing
    precision).
-   Then comes the timer period, measured in milliseconds.
-   Finally there is the identifying information for the timer. Function
    timers have an informative label. Interface and observer timers only
    have an address, which is less useful, but they are uncommon enough
    that this usually doesn\'t matter much.

The above example shows only timers from C++ within Gecko. There are
also timers for `setTimer` or `setInterval` calls in JavaScript code, as
the following sample shows.

    -991946880[7f46c365ba00]: [6775]    fn timer (ONE_SHOT     0 ms): [content] chrome://browser/content/tabbrowser.xml:1816:0
    711637568[7f3219c48000]: [6835]    fn timer (ONE_SHOT   100 ms): [content] http://edition.cnn.com/:5:7231
    711637568[7f3219c48000]: [6835]    fn timer (ONE_SHOT   100 ms): [content] http://a.visualrevenue.com/vrs.js:6:9423

These JS timers are annotated with `[content]` and show the JavaScript
source location where they were created. They can come from chrome code
within Firefox, or from web content.

The informative labels are only present on function timers that have had
their creation site annotated. For unannotated function timers, there
are three possible behaviours.

First, on Mac the code uses `dladdr` to get the name immediately, and
the output will look like the following.

    2082435840[100445640]: [81190] fn timer (ONE_SHOT 8000 ms): [from dladdr] gfxFontInfoLoader::DelayedStartCallback(nsITimer*, void*)

Second, on Linux the code uses `dladdr` to get the symbol library and
address, which can be post-processed by `tools/rb/fix_stacks.py`. The
following two lines show the output before and after being
post-processed by that script.

    2088737280[7f606bf68140]: [30710]    fn timer (ONE_SHOT    16 ms): [from dladdr] #0: ???[/home/njn/moz/mi1/o64/dist/bin/libxul.so +0x2144f94]
    2088737280[7f606bf68140]: [30710]    fn timer (ONE_SHOT    16 ms): [from dladdr] #0: mozilla::RefreshDriverTimer::TimerTick(nsITimer*, void*) (/home/njn/moz/mi1/o64/layout/b

Third, on other platforms `dladdr` is not implemented or doesn\'t work
well, and the output will look like the following.

    711637568[7f3219c48000]: [6835]    fn timer (ONE_SHOT    16 ms): ???[dladdr is unimplemented or doesn't work well on this OS]

The `???` indicates that the function timer lacks an explicit name, and
the comment within the square brackets explains why the fallback
mechanism wasn\'t used`.`

If an unannotated timer function appears frequently it is worth
explicitly annotating it so that it will be usefully identified on other
platforms. (Running on Mac or Linux is obviously necessary to learn the
timer function\'s name.) This is done by initializing it with
`initWithNamedFuncCallback` or `initWithNameableFuncCallback` instead of
`initWithNameCallback`.

## Post-processing

TimerFirings logging quickly produces thousands of lines of output. This
output needs post-processing for it to be useful. If the output is
redirected to a file called *`out`*, then the following command will
pull out the timer-related lines, count how many times each unique line
appears, and then print them with the most common ones first.

    cat out | grep timer | sort | uniq -c | sort -r -n

The following is sample output from this command.

        204 801266240[7f7c1f248000]: [7163]    fn timer (ONE_SHOT    50 ms): [content] http://widgets.outbrain.com/outbrain.js:20:330
        135 -495057024[7f74e105ba00]: [7108]    fn timer (ONE_SHOT     4 ms): [content] https://self-repair.mozilla.org/en-US/repair/:7:13669
        118 801266240[7f7c1f248000]: [7163]    fn timer (ONE_SHOT   100 ms): [content] http://a.visualrevenue.com/vrs.js:6:9423
        103 801266240[7f7c1f248000]: [7163]    fn timer (ONE_SHOT    50 ms): [content] http://static.dynamicyield.com/scripts/12086/dy-min.js?v=12086:3:3389
         94 801266240[7f7c1f248000]: [7163]    fn timer (ONE_SHOT    50 ms): [content] https://ad.double-click.net/ddm/adi/N7921.1283839CADREON.COM.AU/B9038144.122190976;sz=300x600;click=http://pixel.mathtag.com/click/img?mt_aid=2744535504761193354&mt_id=1895890&mt_adid=148611&mt_sid=973379&mt_exid=9&mt_inapp=0&mt_uuid=353d5460-19f6-4400-9bbd-d0fcc3bcf595&mt_3pck=http%3A//beacon-apac-hkg1.rubiconproject.com/beacon/t/d1f9921d-4e47-448f-b6ba-36cae1c31b65/&redirect=;ord=2744535504761193354?:83:0
         94 801266240[7f7c1f248000]: [7163]    fn timer (ONE_SHOT   160 ms): nsBrowserStatusFilter::TimeoutHandler
         92 -495057024[7f74e105ba00]: [7108]    fn timer (ONE_SHOT   160 ms): nsBrowserStatusFilter::TimeoutHandler

The first column shows how many times the particular line appeared.

It is sometimes useful to pre-process the output by stripping out
certain parts of each line before doing this aggregation step, for
example, by inserting one or more of the following commands into the
command pipeline.

    sed 's/^[^:]\+: //'           # strip thread IDs
    sed 's/\[[0-9]\+\] //'        # strip process IDs
    sed 's/ \+[0-9]\+ ms//'       # strip timer periods

The following is the previous sample output with all three of these
commands added into the pipeline.

        204    fn timer (ONE_SHOT): [content] http://widgets.outbrain.com/outbrain.js:20:330
        186    fn timer (ONE_SHOT): nsBrowserStatusFilter::TimeoutHandler
        138    fn timer (ONE_SHOT): [content] https://self-repair.mozilla.org/en-US/repair/:7:13669
        118    fn timer (ONE_SHOT): [content] http://a.visualrevenue.com/vrs.js:6:9423
        108    fn timer (SLACK): LayerActivityTracker
        104    fn timer (SLACK): nsIDocument::SelectorCache
        104    fn timer (SLACK): CCTimerFired
