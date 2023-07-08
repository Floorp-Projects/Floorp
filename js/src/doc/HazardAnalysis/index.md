# Static Analysis for Rooting and Heap Write Hazards

Treeherder can run two static analysis builds: the full browser (linux64-haz), just the JS shell (linux64-shell-haz). They show up on treeherder as `H` and `SM(H)`.

## Diagnosing a hazard failure

The first step is to look at what sort of hazard is being reported. There are two types that cause the job to fail: stack rooting hazards for garbage collection, and heap write thread safety hazards for stylo.

The summary output will include either the string `<N> rooting hazards detected` or `<N> heap write hazards detected out of <M> allowed`. See the appropriate section below for each.

## Diagnosing a rooting hazards failure

Click on the `H` build link, select the "Artifacts" pane on the bottom left, and download the `public/build/hazards.txt.gz` and `public/build/hazards.html.gz` files. The HTML file is most useful when running the analysis locally, since it will link to the exact parts of the code in question, but it's easier to talk about the text file here.

Example snippet from `hazards.txt`:

    Function 'jsopcode.cpp:uint8 DecompileExpressionFromStack(JSContext*, int32, int32, class JS::Handle<JS::Value>, int8**)' has unrooted 'ed' of type 'ExpressionDecompiler' live across GC call 'uint8 ExpressionDecompiler::decompilePC(uint8*)' at js/src/jsopcode.cpp:1866
        js/src/jsopcode.cpp:1866: Assume(74,75, !__temp_23*, true)
        js/src/jsopcode.cpp:1867: Assign(75,76, return := 0)
        js/src/jsopcode.cpp:1867: Call(76,77, ed.~ExpressionDecompiler())
    GC Function: uint8 ExpressionDecompiler::decompilePC(uint8*)
        JSString* js::ValueToSource(JSContext*, class JS::Handle<JS::Value>)
        uint8 js::Invoke(JSContext*, JS::Value*, JS::Value*, uint32, JS::Value*, class JS::MutableHandle<JS::Value>)
        uint8 js::Invoke(JSContext*, JS::CallArgs, uint32)
        JSScript* JSFunction::getOrCreateScript(JSContext*)
        uint8 JSFunction::createScriptForLazilyInterpretedFunction(JSContext*, class JS::Handle<JSFunction*>)
        uint8 JSRuntime::cloneSelfHostedFunctionScript(JSContext*, class JS::Handle<js::PropertyName*>, class JS::Handle<JSFunction*>)
        JSScript* js::CloneScript(JSContext*, class JS::Handle<JSObject*>, class JS::Handle<JSFunction*>, const class JS::Handle<JSScript*>, uint32)
        JSObject* js::CloneStaticBlockObject(JSContext*, class JS::Handle<JSObject*>, class JS::Handle<js::StaticBlockObject*>)
        js::StaticBlockObject* js::StaticBlockObject::create(js::ExclusiveContext*)
        js::Shape* js::EmptyShape::getInitialShape(js::ExclusiveContext*, js::Class*, js::TaggedProto, JSObject*, JSObject*, uint32, uint32)
        js::Shape* js::EmptyShape::getInitialShape(js::ExclusiveContext*, js::Class*, js::TaggedProto, JSObject*, JSObject*, uint64, uint32)
        js::UnownedBaseShape* js::BaseShape::getUnowned(js::ExclusiveContext*, js::StackBaseShape*)
        js::BaseShape* js_NewGCBaseShape(js::ThreadSafeContext*) [with js::AllowGC allowGC = (js::AllowGC)1u]
        js::BaseShape* js::gc::NewGCThing(js::ThreadSafeContext*, uint32, uint64, uint32) [with T = js::BaseShape; js::AllowGC allowGC = (js::AllowGC)1u; size_t = long unsigned int]
        void js::gc::RunDebugGC(JSContext*)
        void js::MinorGC(JSRuntime*, uint32)
        GC

This means that a rooting hazard was discovered at `js/src/jsopcode.cpp` line 1866, in the function `DecompileExpressionFromStack` (it is prefixed with the filename because it's a static function.) The problem is that there is an unrooted variable `ed` that holds an `ExpressionDecompiler` live across a call to `decompilePC`. "Live" means that the variable is used after the call to `decompilePC` returns. `decompilePC` may trigger a GC according to the static call stack given starting from the line beginning with "`GC Function:`".

The hazard itself has some barely comprehensible `Assume(...)` and `Call(...)` [gibberish][CFG] that describes the exact data flow path of the variable into the function call. That stuff is rarely useful -- usually, you'll only need to look at it if it's complaining about a temporary and you want to know where the temporary came from. The type `ExpressionDecompiler` is believed to hold pointers to GC-controlled objects of some sort. The analysis currently does not describe the exact field it is worried about.

To unpack this a little, the analysis is saying the following can happen:

* `ExpressionDecompiler` contains some pointer to a GC thing. For example, it might have a field `obj` of type `JSObject*`. (There is a `gcTypes.txt` file inside `hazardIntermediates.tar.xz` that will give the detailed explanation for all types.)
* `DecompileExpressionFromStack` is called.
* A pointer is stored in that field of the `ed` variable.
* `decompilePC` is invoked, which calls `ValueToSource`, which calls `Invoke`, which eventually calls `js::MinorGC`
* During the resulting garbage collection, the object pointed to by `ed.obj` is moved to a different location. All pointers stored in the JS heap are updated automatically, as are all rooted pointers. `ed.obj` is not, because the GC doesn't know about it.
* After `decompilePC` returns, something accesses `ed.obj`. This is now a stale pointer, and may refer to just about anything -- the wrong object, an invalid object, or whatever. As TeX would say, **badness 10000**.

## Diagnosing a heap write hazard failure

OBSOLETE: The heap write hazard analysis has not been updated in years and is looking for things that no longer exist, and therefore will always report zero problems.

For the thread unsafe heap write analysis, a hazard means that some Gecko_* function calls, directly or indirectly, code that writes to something on the heap, or calls an unknown function that *might* write to something on the heap. The analysis requires quite a few annotations to describe things that are actually safe. This section will be expanded as we gain more experience with the analysis, but here are some common issues:

* Adding a new Gecko_* function: often, you will need to annotate any outparams or owned (thread-local) parameters in the `treatAsSafeArgument` function in `js/src/devtools/rootAnalysis/analyzeHeapWrites.js`.
* Calling some libc function: if you add a call to some random libc function (eg `sin()` or `floor()` or `ceil()`, though the latter two are already annotated), the analysis will report an "External Function". Add it to `checkExternalFunction`, assuming it *doesn't* have the possibility of writing to shared heap memory.
* If you call some non-returning (crashing) function that the analysis doesn't know about, you'll need to add it to `ignoreContents`.

On the other hand, you might have a real thread safety issue on your hands. Shared caches are common problems. Fix it.

## Analysis implementation

These builds do the following:

* set up a build environment and run the analysis within it, then upload the resulting files
 * compile an optimized JS shell to later run the analysis
 * compile the browser with gcc, using a slightly modified version of the sixgill (http://svn.sixgill.org) gcc plugin
* produce a set of `.xdb` files describing everything encountered during the compilation
* analyze the `.xdb` files with scripts in `js/src/devtools/rootAnalysis`

The format of the information stored in those files is [somewhat documented][CFG].

## Running the analysis

### Pushing to try

The easiest way to run an analysis is to push to try with `mach try fuzzy -q "'haz"` (or, if the hazards of interest are contained entirely within `js/src`, use `mach try fuzzy -q "'shell-haz"` for a much faster result). The expected turnaround time for linux64-haz is just under 1.5 hours (~20 minutes for `hazard-linux64-shell-haz`).

The output will be uploaded and an output file `hazards.txt.xz` will be placed into the "Artifacts" info pane on treeherder.

### Running locally

The rooting [hazard analysis may be run][running] using mach.

## So you broke the analysis by adding a hazard. Now what?

Backout, fix the hazard, or (final resort) update the expected number of hazards in `js/src/devtools/rootAnalysis/expect.browser.json` (but don't do that).

The most common way to fix a hazard is to change the variable to be a `Rooted` type, as described in [RootingAPI.h][rooting]

For more complicated cases, ask on the Matrix channel (see [spidermonkey.dev][spidermonkey] for contact info). If you don't get a response, ping sfink or jonco for rooting hazards, bholley or sfink for heap write hazards.

[running]: running.md
[rooting]: https://searchfox.org/mozilla-central/source/js/public/RootingAPI.h
[spidermonkey]: https://spidermonkey.dev/
[CFG]: CFG.md
