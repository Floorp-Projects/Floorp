## GC and CC logs

Garbage collector (GC) and cycle collector (CC) logs give information
about why various JS and C++ objects are alive in the heap. Garbage
collector logs and cycle collector logs can be analyzed in various ways.
In particular, CC logs can be used to understand why the cycle collector
is keeping an object alive. These logs can either be manually or
automatically generated, and they can be generated in both debug and
non-debug builds.

This logs the contents of the Javascript heap to a file named
`gc-edges-NNNN.log`. It also creates a file named `cc-edges-NNNN.log` to
which it dumps the parts of the heap visible to the cycle collector,
which includes native C++ objects that participate in cycle collection,
as well as JS objects being held alive by those C++ objects.

## Generating logs

### From within Firefox

To manually generate GC and CC logs, navigate to `about:memory` and use
the buttons under \"Save GC & CC logs.\" \"Save concise\" will generate
a smaller CC log, \"Save verbose\" will provide a more detailed CC log.
(The GC log will be the same size in either case.)

With multiprocess Firefox, you can't record logs from the content
process, due to sandboxing. You'll need to disable sandboxing by
setting `MOZ_DISABLE_CONTENT_SANDBOX=t` when you run Firefox.

### From the commandline

TLDR: if you just want shutdown GC/CC logs to debug leaks that happen in
our automated tests, you probably want something along the lines of:

    MOZ_DISABLE_CONTENT_SANDBOX=t MOZ_CC_LOG_DIRECTORY=/full/path/to/log/directory/ MOZ_CC_LOG_SHUTDOWN=1 MOZ_CC_ALL_TRACES=shutdown ./mach ...

As noted in the previous section, with multiprocess Firefox, you can't
record logs from the content process, due to sandboxing. You'll need to
disable sandboxing by setting `MOZ_DISABLE_CONTENT_SANDBOX=t` when you
run Firefox.

On desktop Firefox you can override the default location of the log
files by setting the `MOZ_CC_LOG_DIRECTORY` environment variable. By
default, they go to a temporary directory which differs per OS - it's
`/tmp/` on Linux/BSD, `$LOCALAPPDATA\Temp\` on Windows, and somewhere in
`/var/folders/` on Mac (whatever the directory service returns for
`TmpD`/`NS_OS_TEMP_DIR`). Note that just `MOZ_CC_LOG_DIRECTORY=.` won't
work - you need to specify a full path. On Firefox for Android you can
use the cc-dump.xpi
extension to save the files to `/sdcard`. By default, the file is
created in some temp directory, and the path to the file is printed to
the Error Console.

To log every cycle collection, set the `MOZ_CC_LOG_ALL` environment
variable. To log only shutdown collections, set `MOZ_CC_LOG_SHUTDOWN`.
To make all CCs verbose, set `MOZ_CC_ALL_TRACES to "all`\", or to
\"`shutdown`\" to make only shutdown CCs verbose.

Live GC logging can be enabled with the pref
`javascript.options.mem.log`. Output to a file can be controlled with
the MOZ_GCTIMER environment variable. See the [Statistics
API](https://developer.mozilla.org/en-US/docs/Tools/Tools_Toolbox#settings/en-US/docs/SpiderMonkey/Internals/GC/Statistics_API) page for
details on values.

Set the environment variable `MOZ_CC_LOG_THREAD` to `main` to only log
main thread CCs, or to `worker` to only log worker CCs. The default
value is `all`, which will log all CCs.

To get cycle collector logs on Try server, set `MOZ_CC_LOG_DIRECTORY` to
`MOZ_UPLOAD_DIR`, then set the other variables appropriately to generate
CC logs. The way to set environment variables depends on the test
harness, or you can modify the code in nsCycleCollector to set that
directly. To find the CC logs once the try run has finished, click on
the particular job, then click on \"Job Details\" in the bottom pane in
TreeHerder, and you should see download links.

To set the environment variable, find the `buildBrowserEnv` method in
the Python file for the test suite you are interested in, and add
something like this code to the file:

    browserEnv["MOZ_CC_LOG_DIRECTORY"] = os.environ["MOZ_UPLOAD_DIR"]
    browserEnv["MOZ_CC_LOG_SHUTDOWN"] = "1"

## Analyzing GC and CC logs

There are numerous scripts that analyze GC and CC logs on
[GitHub](https://github.com/amccreight/heapgraph/tree/master/cc)


To find out why an object is being kept alive, the relevant scripts are
`find_roots.py` and `parse_cc_graph.py` (which is called by
`find_roots.py`). Calling `find_roots.py` on a CC log with a specific
object or kind of object will produce paths from rooting objects to the
specified objects. Most big leaks include an `nsGlobalWindow`, so
that's a good class to try if you don't have any better idea.

To fix a leak, the next step is to figure out why the rooting object is
alive. For a C++ object, you need to figure out where the missing
references are from. For a JS object, you need to figure out why the JS
object is reachable from a JS root. For the latter, you can use the
corresponding [`find_roots.py` for
JS](https://github.com/amccreight/heapgraph/tree/master/g)
on the GC log.

## Alternatives

There are two add-ons that can be used to create and analyze CC graphs.

-   [about:cc](https://bugzilla.mozilla.org/show_bug.cgi?id=726346)
    is simple, ugly, but rather powerful.
-   [about:ccdump](https://addons.mozilla.org/en-US/firefox/addon/cycle-collector-analyzer/?src=ss)
    is prettier but a bit slower.
