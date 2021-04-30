This document is old and some of the information is out-of-date. Use
with caution.

## Strategy for finding leaks

When trying to make a particular testcase not leak, I recommend focusing
first on the largest object graphs (since these entrain many smaller
objects), then on smaller reference-counted object graphs, and then on
any remaining individual objects or small object graphs that don't
entrain other objects.

Because (1) large graphs of leaked objects tend to include some objects
pointed to by global variables that confuse GC-based leak detectors,
which can make leaks look smaller (as in [bug
99180](https://bugzilla.mozilla.org/show_bug.cgi?id=99180){.external
.text}) or hide them completely and (2) large graphs of leaked objects
tend to hide smaller ones, it's much better to go after the large
graphs of leaks first.

A good general pattern for finding and fixing leaks is to start with a
task that you want not to leak (for example, reading email). Start
finding and fixing leaks by running part of the task under nsTraceRefcnt
logging, gradually building up from as little as possible to the
complete task, and fixing most of the leaks in the first steps before
adding additional steps. (By most of the leaks, I mean the leaks of
large numbers of different types of objects or leaks of objects that are
known to entrain many non-logged objects such as JS objects. Seeing a
leaked `GlobalWindowImpl`, `nsXULPDGlobalObject`,
`nsXBLDocGlobalObject`, or `nsXPCWrappedJS` is a sign that there could
be significant numbers of JS objects leaked.)

For example, start with bringing up the mail window and closing the
window without doing anything. Then go on to selecting a folder, then
selecting a message, and then other activities one does while reading
mail.

Once you've done this, and it doesn't leak much, then try the action
under trace-malloc or LSAN or Valgrind to find the leaks of smaller
graphs of objects. (When I refer to the size of a graph of objects, I'm
referring to the number of objects, not the size in bytes. Leaking many
copies of a string could be a very large leak, but the object graphs are
small and easy to identify using GC-based leak detection.)

## What leak tools do we have?

  ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ --------------------------------------------------------------------- ---------------------- -------------------------------------------------------
  Tool                                                                                                                                                                                                                                                   Finds                                                                 Platforms              Requires
  Leak tools for large object graphs                                                                                                                                                                                                                                                                                                                  
  [Leak Gauge](leak_gauge.md)                                                                                                                                                 Windows, documents, and docshells only                                All platforms          Any build
  [GC and CC logs](GC_and_CC_logs.md)                                                                                                                                        JS objects, DOM objects, many other kinds of objects                  All platforms          Any build
  Leak tools for medium-size object graphs                                                                                                                                                                                                                                                                                                            
  [BloatView](bloatview.md), [refcount tracing and balancing](refcount_tracing_and_balancing.md)   Objects that implement `nsISupports` or use `MOZ_COUNT_{CTOR,DTOR}`   All tier 1 platforms   Debug build (or build opt with `--enable-logrefcnt`)
  Leak tools for debugging memory growth that is cleaned up on shutdown                                                                                                                                                                                                                                                                               
  ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ --------------------------------------------------------------------- ---------------------- -------------------------------------------------------

## Common leak patterns

When trying to find a leak of reference-counted objects, there are a
number of patterns that could cause the leak:

1.  Ownership cycles. The most common source of hard-to-fix leaks is
    ownership cycles. If you can avoid creating cycles in the first
    place, please do, since it's often hard to be sure to break the
    cycle in every last case. Sometimes these cycles extend through JS
    objects (discussed further below), and since JS is
    garbage-collected, every pointer acts like an owning pointer and the
    potential for fan-out is larger. See [bug
    106860](https://bugzilla.mozilla.org/show_bug.cgi?id=106860){.external
    .text} and [bug
    84136](https://bugzilla.mozilla.org/show_bug.cgi?id=84136){.external
    .text} for examples. (Is this advice still accurate now that we have
    a cycle collector? \--Jesse)
2.  Dropping a reference on the floor by:
    1.  Forgetting to release (because you weren't using `nsCOMPtr`
        when you should have been): See [bug
        99180](https://bugzilla.mozilla.org/show_bug.cgi?id=99180){.external
        .text} or [bug
        93087](https://bugzilla.mozilla.org/show_bug.cgi?id=93087){.external
        .text} for an example or [bug
        28555](https://bugzilla.mozilla.org/show_bug.cgi?id=28555){.external
        .text} for a slightly more interesting one. This is also a
        frequent problem around early returns when not using `nsCOMPtr`.
    2.  Double-AddRef: This happens most often when assigning the result
        of a function that returns an AddRefed pointer (bad!) into an
        `nsCOMPtr` without using `dont_AddRef()`. See [bug
        76091](https://bugzilla.mozilla.org/show_bug.cgi?id=76091){.external
        .text} or [bug
        49648](https://bugzilla.mozilla.org/show_bug.cgi?id=49648){.external
        .text} for an example.
    3.  \[Obscure\] Double-assignment into the same variable: If you
        release a member variable and then assign into it by calling
        another function that does the same thing, you can leak the
        object assigned into the variable by the inner function. (This
        can happen equally with or without `nsCOMPtr`.) See [bug
        38586](https://bugzilla.mozilla.org/show_bug.cgi?id=38586){.external
        .text} and [bug
        287847](https://bugzilla.mozilla.org/show_bug.cgi?id=287847){.external
        .text} for examples.
3.  Dropping a non-refcounted object on the floor (especially one that
    owns references to reference counted objects). See [bug
    109671](https://bugzilla.mozilla.org/show_bug.cgi?id=109671){.external
    .text} for an example.
4.  Destructors that should have been virtual: If you expect to override
    an object's destructor (which includes giving a derived class of it
    an `nsCOMPtr` member variable) and delete that object through a
    pointer to the base class using delete, its destructor better be
    virtual. (But we have many virtual destructors in the codebase that
    don't need to be -- don't do that.)

## Debugging leaks that go through XPConnect

Many large object graphs that leak go through
[XPConnect](http://www.mozilla.org/scriptable/){.external .text}. This
can mean there will be XPConnect wrapper objects showing up as owning
the leaked objects, but it doesn't mean it's XPConnect's fault
(although that [has been known to
happen](https://bugzilla.mozilla.org/show_bug.cgi?id=76102){.external
.text}, it's rare). Debugging leaks that go through XPConnect requires
a basic understanding of what XPConnect does. XPConnect allows an XPCOM
object to be exposed to JavaScript, and it allows certain JavaScript
objects to be exposed to C++ code as normal XPCOM objects.

When a C++ object is exposed to JavaScript (the more common of the two),
an XPCWrappedNative object is created. This wrapper owns a reference to
the native object until the corresponding JavaScript object is
garbage-collected. This means that if there are leaked GC roots from
which the wrapper is reachable, the wrapper will never release its
reference on the native object. While this can be debugged in detail,
the quickest way to solve these problems is often to simply debug the
leaked JS roots. These roots are printed on shutdown in DEBUG builds,
and the name of the root should give the type of object it is associated
with.

One of the most common ways one could leak a JS root is by leaking an
`nsXPCWrappedJS` object. This is the wrapper object in the reverse
direction \-- when a JS object is used to implement an XPCOM interface
and be used transparently by native code. The `nsXPCWrappedJS` object
creates a GC root that exists as long as the wrapper does. The wrapper
itself is just a normal reference-counted object, so a leaked
`nsXPCWrappedJS` can be debugged using the normal refcount-balancer
tools.

If you really need to debug leaks that involve JS objects closely, you
can get detailed printouts of the paths JS uses to mark objects when it
is determining the set of live objects by using the functions added in
[bug
378261](https://bugzilla.mozilla.org/show_bug.cgi?id=378261){.external
.text} and [bug
378255](https://bugzilla.mozilla.org/show_bug.cgi?id=378255){.external
.text}. (More documentation of this replacement for GC_MARK_DEBUG, the
old way of doing it, would be useful. It may just involve setting the
`XPC_SHUTDOWN_HEAP_DUMP` environment variable to a file name, but I
haven't tested that.)

## Post-processing of stack traces

On Mac and Linux, the stack traces generated by our internal debugging
tools don't have very good symbol information (since they just show the
results of `dladdr`). The stacks can be significantly improved (better
symbols, and file name / line number information) by post-processing.
Stacks can be piped through the script `tools/rb/fix_stacks.py` to do
this. These scripts are designed to be run on balance trees in addition
to raw stacks; since they are rather slow, it is often **much faster**
to generate balance trees (e.g., using `make-tree.pl` for the refcount
balancer or `diffbloatdump.pl --use-address` for trace-malloc) and*then*
run the balance trees (which are much smaller) through the
post-processing.

## Getting symbol information for system libraries

### Windows

Setting the environment variable `_NT_SYMBOL_PATH` to something like
`symsrv*symsrv.dll*f:\localsymbols*http://msdl.microsoft.com/download/symbols`
as described in [Microsoft's
article](http://support.microsoft.com/kb/311503){.external .text}. This
needs to be done when running, since we do the address to symbol mapping
at runtime.

### Linux

Many Linux distros provide packages containing external debugging
symbols for system libraries. `fix_stacks.py` uses this debugging
information (although it does not verify that they match the library
versions on the system).

For example, on Fedora, these are in \*-debuginfo RPMs (which are
available in yum repositories that are disabled by default, but easily
enabled by editing the system configuration).

## Tips

### Disabling Arena Allocation

With many lower-level leak tools (particularly trace-malloc based ones,
like leaksoup) it can be helpful to disable arena allocation of objects
that you're interested in, when possible, so that each object is
allocated with a separate call to malloc. Some places you can do this
are:

layout engine
:   Define `DEBUG_TRACEMALLOC_FRAMEARENA` where it is commented out in
    `layout/base/nsPresShell.cpp`

glib
:   Set the environment variable `G_SLICE=always-malloc`

## Other References

-   [Performance
    tools](https://wiki.mozilla.org/Performance:Tools "Performance:Tools")
-   [Leak Debugging Screencasts](https://dbaron.org/mozilla/leak-screencasts/){.external
    .text}
-   [LeakingPages](https://wiki.mozilla.org/LeakingPages "LeakingPages") -
    a list of pages known to leak
-   [mdc:Performance](https://developer.mozilla.org/en/Performance "mdc:Performance"){.extiw} -
    contains documentation for all of our memory profiling and leak
    detection tools
