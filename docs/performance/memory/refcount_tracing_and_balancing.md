# Refcount Tracing and Balancing

Refcount tracing and balancing are advanced techniques for tracking down
leak of refcounted objects found with
[BloatView](bloatview.md). The first step
is to run Firefox with refcount tracing enabled, which produces one or
more log files. Refcount tracing logs calls to `Addref` and `Release`,
preferably for a particular set of classes, including call-stacks in
symbolic form (on platforms that support this). Refcount balancing is a
follow-up step that analyzes the resulting log to help a developer
figure out where refcounting went wrong.

## How to build for refcount tracing

Build with `--enable-debug` or `--enable-logrefcnt`.

## How to run with refcount tracing on 

There are several environment variables that can be used.

First, you select one of three environment variables to choose what kind
of logging you want. You almost certainly want `XPCOM_MEM_REFCNT_LOG`.

NOTE: Due to an issue with the sandbox on Windows (bug
[1345568](https://bugzilla.mozilla.org/show_bug.cgi?id=1345568)
refcount logging currently requires the MOZ_DISABLE_CONTENT_SANDBOX
environment variable to be set.

`XPCOM_MEM_REFCNT_LOG`

Setting this environment variable enables refcount tracing. If you set
this environment variable to the name of a file, the log will be output
to that file. You can also set it to 1 to log to stdout or 2 to log to
stderr, but these logs are large and expensive to capture, so you
probably don't want to do that. **WARNING**: you should never use this
without `XPCOM_MEM_LOG_CLASSES` and/or `XPCOM_MEM_LOG_OBJECTS`, because
without some filtering the logging will be completely useless due to how
slow the browser will run and how large the logs it produces will be.

`XPCOM_MEM_COMPTR_LOG`

This environment variable enables logging of additions and releases of
objects into `nsCOMPtr`s. This requires C++ dynamic casts, so it is not
supported on all platforms. However, having an nsCOMPtr log and using it
in the creation of the balance tree allows AddRef and Release calls that
we know are matched to be eliminated from the tree, so it makes it much
easier to debug reference count leaks of objects that have a large
amount of reference counting traffic.

`XPCOM_MEM_ALLOC_LOG`

For platforms that don't have stack-crawl support, XPCOM supports
logging at the call site to `AddRef`/`Release` using the usual cpp
`__FILE__` and __LINE__ number macro expansion hackery. This results
in slower code, but at least you get some data about where the leaks
might be occurring from.

You must also set one or two additional environment variables,
`XPCOM_MEM_LOG_CLASSES` and `XPCOM_MEM_LOG_OBJECTS,` to reduce the set
of objects being logged, in order to improve performance to something
vaguely tolerable.

`XPCOM_MEM_LOG_CLASSES`

This variable should contain a comma-separated list of names which will
be used to compare against the types of the objects being logged. For
example:

    env XPCOM_MEM_LOG_CLASSES=nsDocShell XPCOM_MEM_REFCNT_LOG=./refcounts.log ./mach run

This will log the `AddRef` and `Release` calls only for instances of
`nsDocShell` while running the browser using `mach`, to a file
`refcounts.log`. Note that setting `XPCOM_MEM_LOG_CLASSES` will also
list the *serial number* of each object that leaked in the "bloat log"
(that is, the file specified by the `XPCOM_MEM_BLOAT_LOG` variable; see
[the BloatView documentation](bloatview.md)
for more details). An object's serial number is simply a unique number,
starting at one, that is assigned to the object when it is allocated.

You may use an object's serial number with the following variable to
further restrict the reference count tracing:

    XPCOM_MEM_LOG_OBJECTS

Set this variable to a comma-separated list of object *serial number* or
ranges of *serial number*, e.g., `1,37-42,73,165` (serial numbers start
from 1, not 0). When this is set, along with `XPCOM_MEM_LOG_CLASSES` and
`XPCOM_MEM_REFCNT_LOG`, a stack track will be generated for *only* the
specific objects that you list. For example,

    env XPCOM_MEM_LOG_CLASSES=nsDocShell XPCOM_MEM_LOG_OBJECTS=2 XPCOM_MEM_REFCNT_LOG=./refcounts.log ./mach run

will log stack traces to `refcounts.log` for the 2nd `nsDocShell` object
that gets allocated, and nothing else.

## **Post-processing step 1: finding the leakers**

First you have to figure out which objects leaked. The script
`tools/rb/find_leakers.py` does this. It grovels through the log file,
and figures out which objects got allocated (it knows because they were
just allocated because they got `AddRef()`-ed and their refcount became
1). It adds them to a list. When it finds an object that got freed (it
knows because its refcount goes to 0), it removes it from the list.
Anything left over is leaked.

The scripts output looks like the following.

    0x00253ab0 (1)
    0x00253ae0 (2)
    0x00253bd0 (4)

The number in parentheses indicates the order in which it was allocated,
if you care. Pick one of these pointers for use with Step 2.

## Post-processing step 2: filtering the log

Once you've picked an object that leaked, you can use
`tools/``rb``/filter-log.pl` to filter the log file to drop the call
stack for other objects; This process reduces the size of the log file
and also improves the performance.

    perl -w tools/rb/filter-log.pl --object 0x00253ab0 < ./refcounts.log > my-leak.log

#### Linux Users

The log file generated on Linux system often lack function names, file
names and line numbers. Linux users need to run a script to fix the call
stack.

    python tools/rb/fix_stacks.py < ./refcounts.log > fixed_stack.log

## **Post-processing step 3: building the balance tree**

Now that you've the log file fully prepared, you can build a *balance
tree*. This process takes all the stack `AddRef()` and `Release()` stack
traces and munges them into a call graph. Each node in the graph
represents a call site. Each call site has a *balance factor*, which is
positive if more `AddRef()`s than `Release()`es have happened at the
site, zero if the number of `AddRef()`s and `Release()`es are equal, and
negative if more `Release()`es than `AddRef()`s have happened at the
site.

To build the balance tree, run `tools/rb/make-tree.pl`, specifying the
object of interest. For example:

    perl -w tools/rb/make-tree.pl --object 0x00253ab0 < my-leak.log

This will build an indented tree that looks something like this (except
probably a lot larger and leafier):

    .root: bal=1
      main: bal=1
        DoSomethingWithFooAndReturnItToo: bal=2
          NS_NewFoo: bal=1

Let's pretend in our toy example that `NS_NewFoo()` is a factory method
that makes a new foo and returns it.
`DoSomethingWithFooAndReturnItToo()` is a method that munges the foo
before returning it to `main()`, the main program.

What this little tree is telling you is that you leak *one refcount*
overall on object `0x00253ab0`. But, more specifically, it shows you
that:

-   `NS_NewFoo()` "leaks" a refcount. This is probably "okay"
    because it's a factory method that creates an `AddRef()`-ed object.
-   `DoSomethingWithFooAndReturnItToo()` leaks *two* refcounts.
    Hmm...this probably isn't okay, especially because...
-   `main()` is back down to leaking *one* refcount.

So from this, we can deduce that `main()` is correctly releasing the
refcount that it got on the object returned from
`DoSomethingWithFooAndReturnItToo()`, so the leak *must* be somewhere in
that function.

So now say we go fix the leak in `DoSomethingWithFooAndReturnItToo()`,
re-run our trace, grovel through the log by hand to find the object that
corresponds to `0x00253ab0` in the new run, and run `make-tree.pl`. What
we'd hope to see is a tree that looks like:

    .root: bal=0
      main: bal=0
        DoSomethingWithFooAndReturnItToo: bal=1
          NS_NewFoo: bal=1

That is, `NS_NewFoo()` "leaks" a single reference count; this leak is
"inherited" by `DoSomethingWithFooAndReturnItToo()`; but is finally
balanced by a `Release()` in `main()`.

## Hints

Clearly, this is an iterative and analytical process. Here are some
tricks that make it easier.

**Check for leaks from smart pointers.** If the leak comes from a smart
pointer that is logged in the XPCOM_MEM_COMPTR_LOG, then
find-comptr-leakers.pl will find the exact stack for you, and you don't
have to look at trees.

**Ignore balanced trees**. The `make-tree.pl` script accepts an option
`--ignore-balanced`, which tells it *not* to bother printing out the
children of a node whose balance factor is zero. This can help remove
some of the clutter from an otherwise noisy tree.

**Ignore matched releases from smart pointers.** If you've checked (see
above) that the leak wasn't from a smart pointer, you can ignore the
references that came from smart pointers (where we can use the pointer
identity of the smart pointer to match the AddRef and the Release). This
requires using an XPCOM_MEM_REFCNT_LOG and an XPCOM_MEM_COMPTR_LOG that
were collected at the same time. For more details, see the [old
documentation](http://www-archive.mozilla.org/performance/leak-tutorial.html)
(which should probably be incorporated here). This is best used with
`--ignore-balanced`

**Play Mah Jongg**. An unbalanced tree is not necessarily an evil thing.
More likely, it indicates that one `AddRef()` is cancelled by another
`Release()` somewhere else in the code. So the game is to try to match
them with one another.

**Exclude Functions.** To aid in this process, you can create an
"excludes file", that lists the name of functions that you want to
exclude from the tree building process (presumably because you've
matched them). `make-tree.pl` has an option `--exclude [file]`, where
`[file]` is a newline-separated list of function names that will be
*excluded* from consideration while building the tree. Specifically, any
call stack that contains that call site will not contribute to the
computation of balance factors in the tree.

## How to instrument your objects for refcount tracing and balancing

The process is the same as [instrumenting them for
[BloatView](bloatview.md]
todo
How_to_instrument_your_objects_for_BloatView,
because BloatView and refcount tracing share underlying infrastructure.
