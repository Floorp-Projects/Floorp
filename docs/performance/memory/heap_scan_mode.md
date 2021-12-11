# DMD heap scan mode

Firefox's DMD heap scan mode tracks the set of all live blocks of
malloc-allocated memory and their allocation stacks, and allows you to
log these blocks, and the values stored in them, to a file. When
combined with cycle collector logging, this can be used to investigate
leaks of refcounted cycle collected objects, by figuring out what holds
a strong reference to a leaked object.

**When should you use this?** DMD heap scan mode is intended to be used
to investigate leaks of cycle collected (CCed) objects. DMD heap scan
mode is a "tool of last resort" that should only be used when all
other avenues have been tried and failed, except possibly [ref count
logging](refcount_tracing_and_balancing.md).
It is particularly useful if you have no idea what is causing the leak.
If you have a patch that introduces a leak, you are probably better off
auditing all of the strong references that your patch creates before
trying this.

The particular steps given below are intended for the case where the
leaked object is alive all the way through shutdown. You could modify
these steps for leaks that go away in shutdown by collecting a CC and
DMD log prior to shutdown. However, in that case it may be easier to use
refcount logging, or rr with a conditional breakpoint set on calls to
`Release()` for the leaking object, to see what object actually does the
release that causes the leaked object to go away.

## Prerequisites

-   A debug DMD build of Firefox. [This
    page](dmd.md)
    describes how to do that. This should probably be an optimized
    build. Non-optimized DMD builds will generate better stack traces,
    but they can be so slow as to be useless.
-   The build is going to be very slow, so you may need to disable some
    shutdown checks. First, in
    `toolkit/components/terminator/nsTerminator.cpp`, delete everything
    in `RunWatchDog` but the call to `NS_SetCurrentThreadName`. This
    will keep the watch dog from killing the browser when shut down
    takes multiple minutes. Secondly, you may need to comment out the
    call to `MOZ_CRASH("NSS_Shutdown failed");` in
    `xpcom/build/XPCOMInit.cpp`, as this also seems to trigger when
    shutdown is extremely slow.
-   You need the cycle collector analysis script `find_roots.py`, which
    can be downloaded as part of [this repo on
    Github](https://github.com/amccreight/heapgraph).

## Generating Logs

The next step is to generate a number of log files. You need to get a
shutdown CC log and a DMD log, for a single run.

**Definitions** I'll write `$objdir` for the object directory for your
Firefox DMD build, `$srcdir` for the top level of the Firefox source
directory, and `$heapgraph` for the location of the heapgraph repo, and
`$logdir` for the location you want logs to go to. `$logdir` should end
in a path separator. For instance, `~/logs/leak/`.

The command you need to run Firefox will look something like this:

    XPCOM_MEM_BLOAT_LOG=1 MOZ_CC_LOG_SHUTDOWN=1 MOZ_DISABLE_CONTENT_SANDBOX=t MOZ_CC_LOG_DIRECTORY=$logdir 
    MOZ_CC_LOG_PROCESS=content MOZ_CC_LOG_THREAD=main MOZ_DMD_SHUTDOWN_LOG=$logdir MOZ_DMD_LOG_PROCESS=tab ./mach run --dmd --mode=scan

Breaking this down:

-   `XPCOM_MEM_BLOAT_LOG=1`: This reports a list of the counts of every
    object created and destroyed and tracked by the XPCOM leak tracking
    system. From this chart, you can see how many objects of a
    particular type were leaked through shutdown. This can come in handy
    during the manual analysis phase later, to get evidence to support
    your hunches. For instance, if you think that an `nsFoo` object
    might be holding your leaking object alive, you can use this to
    easily see if we leaked an `nsFoo` object.
-   `MOZ_CC_LOG_SHUTDOWN=1`: This generates a cycle collector log during
    shutdown. Creating this log during shutdown is nice because there
    are less things unrelated to the leak in the log, and various cycle
    collector optimizations are disabled. A garbage collector log will
    also be created, which you may not need.
-   `MOZ_DISABLE_CONTENT_SANDBOX=t`: This disables the content process
    sandbox, which is needed because the DMD and CC log files are
    created directly by the child processes.
-   `MOZ_CC_LOG_DIRECTORY=$logdir`: This selects the location for cycle
    collector logs to be saved.
-   `MOZ_CC_LOG_PROCESS=content MOZ_CC_LOG_THREAD=main`: These options
    specify that we only want CC logs for the main thread of content
    processes, to make shutdown less slow. If your leak is happening in
    a different process or thread, change the options, which are listed
    in `xpcom/base/nsCycleCollector.cpp`.
-   `MOZ_DMD_SHUTDOWN_LOG=$logdir`: This option specifies that we want a
    DMD log to be taken very late in XPCOM shutdown, and the location
    for that log to be saved. Like with the CC log, we want this log
    very late to avoid as many non-leaking things as possible.
-   `MOZ_DMD_LOG_PROCESS=tab`: As with the CC, this means that we only
    want these logs in content processes, in order to make shutdown
    faster. The allowed values here are the same as those returned by
    `XRE_GetProcessType()`, so adjust as needed.
-   Finally, the `--dmd` option need to be passed in so that DMD will be
    run. `--mode=scan` is needed so that when we get a DMD log the
    entire contents of each block of memory is saved for later analysis.

With that command line in hand, you can start Firefox. Be aware that
this may take multiple minutes if you have optimization disabled.

Once it has started, go through the steps you need to reproduce your
leak. If your leak is a ghost window, it can be handy to get an
`about:memory` report and write down the PID of the leaking process. You
may want to wait 10 or so seconds after this to make sure as much as
possible is cleaned up.

Next, exit the browser. This will cause a lot of logs to be written out,
so it can take a while.

## Analyzing the Logs

##### Getting the PID and address of the leaking object

The first step is to figure out the **PID** of the leaking process. The
second step is to figure out **the address of the leaking object**,
usually a window. Conveniently, you can usually do both at once using
the cycle collector log. If you are investigating a leak of
`www.example.com`, then from `$logdir` you can do
`"grep nsGlobalWindow cc-edges* | grep example.com"`. This looks through
all of the windows in all of the CC logs (which may leaked, this late in
shutdown), and then filters out windows where the URL contains
`example.com`.

The result of that grep will contain output that looks something like
this:

    cc-edges.15873.log:0x7f0897082c00 [rc=1285] nsGlobalWindowInner # 2147483662 inner https://www.example.com/

cc-edges.15873.log: The first part is the file name where it was
found. `15873` is the PID of the process that leaked. You'll want to
write down the name of the file and the PID. Let's call the file
`$cclog` and the pid `$pid`.

0x7f0897082c00: This is the address of the leaking window. You'll
also want to write that down. Let's call this `$winaddr`.

If there are multiple files, you'll end up with one that looks like
`cc-edges.$pid.log` and one or more that look like
`cc-edges.$pid-$n.log` for various values of `$n`. You want the one with
the largest `$n`, as this was recorded the latest, and so it will
contain the least non-garbage.

##### Identifying the root in the cycle collector log

The next step is to figure out why the cycle collector could not collect
the window, using the `find_roots.py` script from the heapgraph
repository. The command to invoke this looks like this:

    python $heapgraph/find_roots.py $cclog $winaddr

This may take a few seconds. It will eventually produce some output.
You'll want to save a copy of this output for later.

The output will look something like this, after a message about loading
progress:

    0x7f0882fe3230 [FragmentOrElement (xhtml) script https://www.example.com]
    --[[via hash] mListenerManager]--> 0x7f0899b4e550 [EventListenerManager]
    --[mListeners event=onload listenerType=3 [i]]--> 0x7f0882ff8f80 [CallbackObject]
    --[mIncumbentGlobal]--> 0x7f0897082c00 [nsGlobalWindowInner # 2147483662 inner https://www.example.com]

Root 0x7f0882fe3230 is a ref counted object with 1 unknown edge(s).
    known edges:
    0x7f08975a24c0 [FragmentOrElement (xhtml) head https://www.example.com] --[mAttrsAndChildren[i]]--> 0x7f0882fe3230
    0x7f08967e7b20 [JS Object (HTMLScriptElement)] --[UnwrapDOMObject(obj)]--> 0x7f0882fe3230

The first two lines mean that the script element `0x7f0882fe3230`
contains a strong reference to the EventListenerManager
`0x7f0899b4e550`. "[via hash] mListenerManager" is a description of
that strong reference. Together, these lines show a chain of strong
references from an object the cycle collector thinks needs to be kept
alive, `0x7f0899b4e550`, to the object` 0x7f0897082c00` that you asked
about. Most of the time, the actual chain is not important, because the
cycle collector can only tell us about what went right. Let us call the
address of the leaking object (`0x7f0882fe3230` in this case)
`$leakaddr`.

Besides `$leakaddr`, the other interesting part is the chunk at the
bottom. It tells us that there is 1 unknown edge, and 2 known edges.
What this means is that the leaking object has a refcount of 3, but the
cycle collector was only told about these two references. In this case,
a head element and a JS object (the JS reflector of the script element).
We need to figure out what the unknown reference is from, as that is
where our leak really is.

##### Figure out what is holding the leaking object alive.

Now we need to use the DMD heap scan logs. These contain the contents of
every live block of memory.

The first step to using the DMD heap scan logs is to do some
pre-processing for the DMD log. Stacks need to be symbolicated, and we
need to clamp the values contained in the heap. Clamping is the same
kind of analysis that a conservative GC does: if a word-aligned value in
a heap block points to somewhere within another heap block, replace that
value with the address of the block.

Both kinds of preprocessing are done by the `dmd.py` script, which can
be invoked like this:

    $objdir/dist/bin/dmd.py --clamp-contents dmd-$pid.log.gz

This can take a few minutes due to symbolification, but you only need to
run it once on a log file.

After that is done, we can finally find out which objects (possibly)
point to other objects, using the block_analyzer script:

    python $srcdir/memory/replace/dmd/block_analyzer.py dmd-$pid.log.gz $leakaddr

This will look through every block of memory in the log, and give some
basic information about any block of memory that (possibly) contains a
pointer to that object. You can pass some additional options to affect
how the results are displayed. "-sfl 10000 -a" is useful. The -sfl 10000
tells it to not truncate stack frames, and -a tells it to not display
generic frames related to the allocator.

Caveat: I think block_analyzer.py does not attempt to clamp the address
you pass into it, so if it is an offset from the start of the block, it
won't find it.

    block_analyzer.py` will return a series of entries that look like this
    with the [...] indicating where I have removed things):
    0x7f089306b000 size = 4096 bytes at byte offset 2168
    nsAttrAndChildArray::GrowBy[...]
    nsAttrAndChildArray::InsertChildAt[...]
    [...]

`0x7f089306b000` is the address of the block that contains `$leakaddr`.
144 bytes is the size of that block. That can be useful for confirming
your guess about what class the block actually is. The byte offset tells
you were in the block the pointer is. This is mostly useful for larger
objects, and you can potentially combine this with debugging information
to figure out exactly what field this is. The rest of the entry is the
stack trace for the allocation of the block, which is the most useful
piece of information.

What you need to do now is to go through every one of these entries and
place it into three categories: strong reference known to the cycle
collector, weak reference, or something else! The goal is to eventually
shrink down the "something else" category until there are only as many
things in it as there are unknown references to the leaking object, and
then you have your leaker.

To place an entry into one of the categories, you must look at the code
locations given in the stack trace, and see if you can tell what the
object is based on that, then compare that to what `find_roots.py` told
you.

For instance, one of the strong references in the CC log is from a head
element to its child via `mAttrsAndChildren`, and that sounds a lot like
this, so we can mark it as being a strong known reference.

This is an iterative process, where you first go through and mark off
the things that are easily categorizable, and repeat until you have a
small list of things to analyze.

##### Example analysis of block_analyzer.py results

In one debugging session where I was investigating the leak from bug
1451985, I eventually reduced the list of entries until this was the
most suspicious looking entry:

    0x7f0892f29630 size = 392 bytes at byte offset 56
    mozilla::dom::ScriptLoader::ProcessExternalScript[...]
    [...]

I went to that line of `ScriptLoader::ProcessExternalScript()`, and it
contained a call to ScriptLoader::CreateLoadRequest(). Fortunately, this
method mostly just contains two calls to `new`, one for
`ScriptLoadRequest` and one for `ModuleLoadRequest`. (This is where an
unoptimized build comes in handy, as it would have pointed out the exact
line. Unfortunately, in this particular case, the unoptimized build was
so slow I wasn't getting any logs.) I then looked through the list of
leaked objects generated by `XPCOM_MEM_BLOAT_LOG` and saw that we were
leaking a `ScriptLoadRequest`, so I went and looked at its class
definition, where I noticed that `ScriptLoadRequest` had a strong
reference to an element that it wasn't telling the cycle collector
about, which seemed suspicious.

The first thing I did to try to confirm that this was the source of the
leak was pass the address of this object into the cycle collector
analysis log, `find_roots.py`, that we used at an earlier step. That
gave a result that contained this:

    0x7f0882fe3230 [FragmentOrElement (xhtml) script [...]
    --[mNodeInfo]--> 0x7f0897431f00 [NodeInfo (xhtml) script]
    [...]
    --[mLoadingAsyncRequests]--> 0x7f0892f29630 [ScriptLoadRequest]

This confirms that this block is actually a ScriptLoadRequest. Secondly,
notice that the load request is being held alive by the very same script
element that is causing the window leak! This strongly suggests that
there is a cycle of strong references between the script element and the
load request. I then added the missing field to the traverse and unlink
methods of ScriptLoadRequest, and confirmed that I couldn't reproduce
the leak.

Keep in mind that you may need to run `block_analyzer.py` multiple
times. For instance, if the script element was being held alive by some
container being held alive by a runnable, we'd first need to figure out
that the container was holding the element. If it isn't possible to
figure out what is holding that alive, you'd have to run block_analyzer
again. This isn't too bad, because unlike ref count logging, we have the
full state of memory in our existing log, so we don't need to run the
browser again.
