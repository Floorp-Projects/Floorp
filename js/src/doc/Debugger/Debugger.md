# The Debugger Object

When called as a constructor, the `Debugger` object creates a new
`Debugger` instance.

### `new Debugger([global, ...])`
Create a debugger object, and apply its [`addDebuggee`][add] method to
each of the given <i>global</i> objects to add them as the initial
debuggees.

## Accessor Properties of the Debugger Prototype Object

A `Debugger` instance inherits the following accessor properties from
its prototype:

### `allowUnobservedAsmJS`
A boolean value indicating whether asm.js code running inside this
`Debugger` instance's debuggee globals is invisible to Debugger API
handlers and breakpoints. Setting this to `false` inhibits the
ahead-of-time asm.js compiler and forces asm.js code to run as normal
JavaScript. This is an accessor property with a getter and setter. It is
initially `false` in a freshly created `Debugger` instance.

Setting this flag to `true` is intended for uses of subsystems of the
Debugger API (e.g, [`Debugger.Source`][source]) for purposes other than
step debugging a target JavaScript program.

### `collectCoverageInfo`
A boolean value indicating whether code coverage should be enabled inside
each debuggee of this `Debugger` instance. Changing this flag value will
recompile all JIT code to add or remove code coverage
instrumentation. Changing this flag when any frame of the debuggee is
currently active on the stack will produce an exception.

Setting this to `true` enables code coverage instrumentation, which can be
accessed via the [`Debugger.Script`][script] `getOffsetsCoverage`
function. In some cases, the code coverage might expose information which
pre-date the modification of this flag. Code coverage reports are monotone,
thus one can take a snapshot when coverage first is enabled, and output the
difference.

Setting this to `false` prevents this `Debugger` instance from requiring any
code coverage instrumentation, but it does not guarantee that the
instrumentation is not present.

### `uncaughtExceptionHook`
Either `null` or a function that SpiderMonkey calls when a call to a
debug event handler, breakpoint handler, or similar
function throws some exception, which we refer to as
<i>debugger-exception</i> here. Exceptions thrown in the debugger are
not propagated to debuggee code; instead, SpiderMonkey calls this
function, passing <i>debugger-exception</i> as its sole argument and
the `Debugger` instance as the `this` value. This function should
return a [resumption value][rv], which determines how the debuggee
should continue.

If the uncaught exception hook itself throws an exception,
<i>uncaught-hook-exception</i>, SpiderMonkey throws a new error object,
<i>confess-to-debuggee-exception</i>, to the debuggee whose message
blames the debugger, and includes textual descriptions of
<i>uncaught-hook-exception</i> and the original
<i>debugger-exception</i>.

If `uncaughtExceptionHook`'s value is `null`, SpiderMonkey throws an
exception to the debuggee whose message blames the debugger, and
includes a textual description of <i>debugger-exception</i>.

Assigning anything other than a callable value or `null` to this
property throws a `TypeError` exception.

(This is not an ideal way to handle debugger bugs, but the hope here is
that some sort of backstop, even if imperfect, will make life easier for
debugger developers. For example, an uncaught exception hook may have
access to browser-level features like the `alert` function, which this
API's implementation does not, making it possible to present debugger
errors to the developer in a way suited to the context.)


## Debugger Handler Functions

Each `Debugger` instance inherits accessor properties with which you can
store handler functions for SpiderMonkey to call when given events occur
in debuggee code.

When one of the events described below occurs in debuggee code, the engine
pauses the debuggee and calls the corresponding debugging handler on each
`Debugger` instance that is observing the debuggee. The handler functions
receive the `Debugger` instance as their `this` value. Most handler
functions can return a [resumption value][rv] indicating how the debuggee's
execution should proceed.

On a new `Debugger` instance, each of these properties is initially
`undefined`. Any value assigned to a debugging handler must be either a
function or `undefined`; otherwise a `TypeError` is thrown.

Handler functions run in the same thread in which the event occurred.
They run in the compartment to which they belong, not in a debuggee
compartment.

### `onNewScript(script, global)`
New code, represented by the [`Debugger.Script`][script] instance
<i>script</i>, has been loaded in the scope of the debuggees.

Since each function has its own [`Debugger.Script`][script], separate from
the top-level code or function that encloses it, loading JavaScript code
typically introduces not just a single script, but a tree of scripts
representing the top-level code and any functions it includes. The
`onNewScript` hook reports only the root script of such a tree. If
necessary, the handler function can use the scripts' `getChildScripts`
method to walk the tree and obtain all the newly introduced scripts.

This method's return value is ignored.

### `onNewPromise(promise)`
A new Promise object, referenced by the [`Debugger.Object`][object] instance
*promise*, has been allocated in the scope of the debuggees. The Promise's
allocation stack can be obtained using the *promiseAllocationStack*
accessor property of the [`Debugger.Object`][object] instance *promise*.

This handler method should return a [resumption value][rv] specifying how
the debuggee's execution should proceed. However, note that a <code>{
return: <i>value</i> }</code> resumption value is treated like `undefined`
("continue normally"); <i>value</i> is ignored.

### `onPromiseSettled(promise)`
A Promise object, referenced by the [`Debugger.Object`][object] instance
*promise* that was allocated within a debuggee scope, has settled (either
fulfilled or rejected). The Promise's state, fulfillment or rejection
value, and the allocation and resolution stacks can be obtained using the
Promise-related accessor properties of the [`Debugger.Object`][object]
instance *promise*.

This handler method should return a [resumption value][rv] specifying how
the debuggee's execution should proceed. However, note that a <code>{
return: <i>value</i> }</code> resumption value is treated like `undefined`
("continue normally"); <i>value</i> is ignored.

### `onDebuggerStatement(frame)`
Debuggee code has executed a <i>debugger</i> statement in <i>frame</i>.
This method should return a [resumption value][rv] specifying how the
debuggee's execution should proceed.

### `onEnterFrame(frame)`
The stack frame <i>frame</i> is about to begin executing code.
(Naturally, <i>frame</i> is currently the youngest
[visible frame][vf].) This method should return
a [resumption value][rv] specifying how the debuggee's execution should
proceed.

SpiderMonkey only calls `onEnterFrame` to report
[visible][vf], non-`"debugger"` frames.

### `onNativeCall(callee, reason)`
A call to a native function is being made from a debuggee realm.
<i>callee</i> is a [`Debugger.Object`] for the function being called, and
<i>reason</i> is a string describing the reason the call was made, and
has one of the following values:

`get`: The native is the getter for a property which is being accessed.
`set`: The native is the setter for a property being written to.
`call`: Any call not fitting into the above categories.

This method should return a [resumption value][rv] specifying how the debuggee's
execution should proceed. If a return value is overridden for a constructor
call, it must be an object.

SpiderMonkey only calls `onNativeCall` hooks when execution is inside a
debugger evaluation associated with the debugger that has the `onNativeCall`
hook.  Such evaluation methods include `Debugger.Object.executeInGlobal`,
`Debugger.Frame.eval`, and associated methods.

Separately, any Debugger hooks triggered during calls to
`Debugger.Object.executeInGlobal`, `Debugger.Frame.eval`, and associated methods
will only be triggered on Debugger objects owned by the Debugger performing
the evaluation.

### `onExceptionUnwind(frame, value)`
The exception <i>value</i> has been thrown, and has propagated to
<i>frame</i>; <i>frame</i> is the youngest remaining stack frame, and is a
debuggee frame. This method should return a [resumption value][rv]
specifying how the debuggee's execution should proceed. If it returns
`undefined`, the exception continues to propagate as normal: if control in
`frame` is in a `try` block, control jumps to the corresponding `catch` or
`finally` block; otherwise, <i>frame</i> is popped, and the exception
propagates to <i>frame</i>'s caller.

When an exception's propagation causes control to enter a `finally`
block, the exception is temporarily set aside. If the `finally` block
finishes normally, the exception resumes propagation, and the debugger's
`onExceptionUnwind` handler is called again, in the same frame. (The
other possibility is for the `finally` block to exit due to a `return`,
`continue`, or `break` statement, or a new exception. In those cases the
old exception does not continue to propagate; it is discarded.)

This handler is not called when unwinding a frame due to an over-recursion
or out-of-memory exception.

### `sourceHandler(ASuffusionOfYellow)`
This method is never called. If it is ever called, a contradiction has
been proven, and the debugger is free to assume that everything is true.

### `onError(frame, report)`
SpiderMonkey is about to report an error in <i>frame</i>. <i>Report</i>
is an object describing the error, with the following properties:

* `message`
  The fully formatted error message.

* `file`
  If present, the source file name, URL, etc. (If this property is
  present, the <i>line</i> property will be too, and vice versa.)

* `line`
  If present, the source line number at which the error occurred.

* `lineText`
  If present, this is the source code of the offending line.

* `offset`
  The index of the character within lineText at which the error occurred.

* `warning`
  Present and true if this is a warning; absent otherwise.

* `strict`
  Present and true if this error or warning is due to the strict option
  (not to be confused with ES strict mode)

* `exception`
  Present and true if an exception will be thrown; absent otherwise.

* `arguments`
  An array of strings, representing the arguments substituted into the
  error message.

This method's return value is ignored.

### `onNewGlobalObject(global)`
A new global object, <i>global</i>, has been created.

This handler method should return a [resumption value][rv] specifying how
the debuggee's execution should proceed. However, note that a <code>{ return:
<i>value</i> }</code> resumption value is treated like `undefined` ("continue
normally"); <i>value</i> is ignored. (Allowing the handler to substitute
its own value for the new global object doesn't seem useful.)

This handler method is only available to debuggers running in privileged
code ("chrome", in Firefox). Most functions provided by this `Debugger`
API observe activity in only those globals that are reachable by the
API's user, thus imposing capability-based restrictions on a
`Debugger`'s reach. However, the `onNewGlobalObject` method allows the
API user to monitor all global object creation that occurs anywhere
within the JavaScript system (the "JSRuntime", in SpiderMonkey terms),
thereby escaping the capability-based limits. For this reason,
`onNewGlobalObject` is only available to privileged code.

Note that, even though the presence of a `Debugger`'s `onNewGlobalObject`
hook can have arbitrary side effects, the garbage collector does not
consider the presence of the hook sufficient reason to keep the `Debugger`
alive. Thus, the behavior of code that uses `onNewGlobalObject` on unrooted
`Debugger`s may be affected by the garbage collector's activity, and
is not entirely deterministic.


## Function Properties of the Debugger Prototype Object

The functions described below may only be called with a `this` value
referring to a `Debugger` instance; they may not be used as methods of
other kinds of objects.

### `addDebuggee(global)`
Add the global object designated by <i>global</i> to the set of global
objects this `Debugger` instance is debugging. If the designated global
is already a debuggee, this has no effect. Return this `Debugger`'s
[`Debugger.Object`][object] instance referring to the designated global.

The value <i>global</i> may be any of the following:

* A global object.

* An HTML5 `WindowProxy` object (an "outer window", in Firefox
    terminology), which is treated as if the `Window` object of the
    browsing context's active document (the "inner window") were passed.

* A cross-compartment wrapper of an object; we apply the prior rules to
    the wrapped object.

* A [`Debugger.Object`][object] instance belonging to this `Debugger` instance;
    we apply the prior rules to the referent.

* Any other sort of value is treated as a `TypeError`. (Note that each
    rule is only applied once in the process of resolving a given
    <i>global</i> argument. Thus, for example, a [`Debugger.Object`][object]
    referring to a second [`Debugger.Object`][object] which refers to a global does
    not designate that global for the purposes of this function.)

The global designated by <i>global</i> must be in a different
compartment than this `Debugger` instance itself. If adding the
designated global's compartment would create a cycle of debugger and
debuggee compartments, this method throws an error.

This method returns the [`Debugger.Object`][object] instance whose referent is
the designated global object.

The `Debugger` instance does not hold a strong reference to its
debuggee globals: if a debuggee global is not otherwise reachable, then
it is dropped from the `Debugger`'s set of debuggees. (Naturally, the
[`Debugger.Object`][object] instance this method returns does hold a strong
reference to the added global.)

If this debugger is [tracking allocation sites][tracking-allocs] and cannot
track allocation sites for <i>global</i>, this method throws an `Error`.

### `addAllGlobalsAsDebuggees()`
This method is like [`addDebuggee`][add], but adds all the global
objects from all compartments to this `Debugger` instance's set of
debuggees. Note that it skips this debugger's compartment.

If this debugger is [tracking allocation sites][tracking-allocs] and cannot
track allocation sites for some global, this method throws an `Error`.
Otherwise this method returns `undefined`.

This method is only available to debuggers running in privileged
code ("chrome", in Firefox). Most functions provided by this `Debugger`
API observe activity in only those globals that are reachable by the
API's user, thus imposing capability-based restrictions on a
`Debugger`'s reach. However, the `addAllGlobalsAsDebuggees` method
allows the API user to monitor all global object creation that
occurs anywhere within the JavaScript system (the "JSRuntime", in
SpiderMonkey terms), thereby escaping the capability-based
limits. For this reason, `addAllGlobalsAsDebuggees` is only
available to privileged code.

### `removeDebuggee(global)`
Remove the global object designated by <i>global</i> from this
`Debugger` instance's set of debuggees. Return `undefined`.

This method interprets <i>global</i> using the same rules that
[`addDebuggee`][add] does.

Removing a global as a debuggee from this `Debugger` clears all breakpoints
that belong to that `Debugger` in that global.

### `removeAllDebuggees()`
Remove all the global objects from this `Debugger` instance's set
of debuggees.  Return `undefined`.

### `hasDebuggee(global)`
Return `true` if the global object designated by <i>global</i> is a
debuggee of this `Debugger` instance.

This method interprets <i>global</i> using the same rules that
[`addDebuggee`][add] does.

### `getDebuggees()`
Return an array of distinct [`Debugger.Object`][object] instances whose referents
are all the global objects this `Debugger` instance is debugging.

Since `Debugger` instances don't hold strong references to their
debuggee globals, if a debuggee global is otherwise unreachable, it may
be dropped at any moment from the array this method returns.

### `getNewestFrame()`
Return a [`Debugger.Frame`][frame] instance referring to the youngest
[visible frame][vf] currently on the calling thread's stack, or `null`
if there are no visible frames on the stack.

### `findSources()`
Return an array of all [`Debugger.Source`][source] instances of all debuggee
scripts.

Note that the result may include sources that can no longer ever be
used by the debuggee: say, eval code that has finished running, or
source for unreachable functions. Whether such sources appear can be
affected by the garbage collector's behavior, so this function's result
is not entirely deterministic.

### `findScripts([query])`
Return an array of [`Debugger.Script`][script] instances for all debuggee scripts
matching <i>query</i>. Each instance appears only once in the array.
<i>Query</i> is an object whose properties restrict which scripts are
returned; a script must meet all the criteria given by <i>query</i> to
be returned. If <i>query</i> is omitted, we return the [`Debugger.Script`][script]
instances for all debuggee scripts.

<i>Query</i> may have the following properties:

* `url`

  The script's `url` property must be equal to this value.

* `source`

  The script's `source` property must be equal to this value.

* `line`

  The script must at least partially cover the given source line. If this
  property is present, the `url` property must be present as well.

* `column`

  The script must include given column on the line given by the `line`property.
  If this property is present, the `url` and `line` properties must both be
  present as well.

* `innermost`

  If this property is present and true, the script must be the innermost
  script covering the given source location; scripts of enclosing code are
  omitted.

* `global`

  The script must be in the scope of the given global object. If this
  property's value is a [`Debugger.Object`][object] instance belonging to this
  `Debugger` instance, then its referent is used. If the object is not a
  global object, then the global in whose scope it was allocated is used.

All properties of <i>query</i> are optional. Passing an empty object
returns all debuggee code scripts.

Note that the result may include [`Debugger.Script`][script] instances for
scripts that can no longer ever be used by the debuggee, say, those for
eval code that has finished running, or unreachable functions. Whether
such scripts appear can be affected by the garbage collector's
behavior, so this function's behavior is not entirely deterministic.

### `findSourceURLs()`

Return an array of strings containing the URLs of all known sources that
have been created in any debuggee realm.  The array will have one entry for
each source, so may have duplicates.  The URLs for the realms are
occasionally purged and the returned array might not be complete.

### `findObjects([query])`
Return an array of [`Debugger.Object`][object] instances referring to each
live object allocated in the scope of the debuggee globals that matches
*query*. Each instance appears only once in the array. *Query* is an object
whose properties restrict which objects are returned; an object must meet
all the criteria given by *query* to be returned. If *query* is omitted, we
return the [`Debugger.Object`][object] instances for all objects allocated
in the scope of debuggee globals.

The *query* object may have the following properties:

* `class`

  If present, only return objects whose internal `[[Class]]`'s name
  matches the given string. Note that in some cases, the prototype object
  for a given constructor has the same `[[Class]]` as the instances that
  refer to it, but cannot itself be used as a valid instance of the
  class. Code gathering objects by class name may need to examine them
  further before trying to use them.

All properties of *query* are optional. Passing an empty object returns all
objects in debuggee globals.

Unlike `findScripts`, this function is deterministic and will never return
[`Debugger.Object`s][object] referring to previously unreachable objects
that had not been collected yet.

### `clearBreakpoint(handler)`
Remove all breakpoints set in this `Debugger` instance that use
<i>handler</i> as their handler. Note that, if breakpoints using other
handler objects are set at the same location(s) as <i>handler</i>, they
remain in place.

### `clearAllBreakpoints()`
Remove all breakpoints set using this `Debugger` instance.

### `findAllGlobals()`
Return an array of [`Debugger.Object`][object] instances referring to all the
global objects present in this JavaScript instance.

The results of this call can be affected in non-deterministic ways by
the details of the JavaScript implementation. The array may include
[`Debugger.Object`][object] instances referring to global objects that are not
actually reachable by the debuggee or any other code in the system.
(Naturally, once the function has returned, the array's
[`Debugger.Object`][object] instances strongly reference the globals they refer
to.)

This handler method is only available to debuggers running in privileged
code ("chrome", in Firefox). Most functions provided by this `Debugger`
API observe activity in only those globals that are reachable by the
API's user, thus imposing capability-based restrictions on a
`Debugger`'s reach. However, `findAllGlobals` allows the API user to
find all global objects anywhere within the JavaScript system (the
"JSRuntime", in SpiderMonkey terms), thereby escaping the
capability-based limits. For this reason, `findAllGlobals` is only
available to privileged code.

### `makeGlobalObjectReference(global)`
Return the [`Debugger.Object`][object] whose referent is the global object
designated by <i>global</i>, without adding the designated global as a
debuggee. If <i>global</i> does not designate a global object, throw a
`TypeError`. Determine which global is designated by <i>global</i>
using the same rules as [`Debugger.prototype.addDebuggee`][add].

### `adoptDebuggeeValue(value)`
Given a debuggee value `value` owned by an arbitrary `Debugger`, return an
equivalent debuggee value owned by this `Debugger`.

If `value` is a primitive value, return it unchanged. If `value` is a
`Debugger.Object` owned by an arbitrary `Debugger`, return an equivalent
`Debugger.Object` owned by this `Debugger`. Otherwise, if `value` is some
other kind of object, and hence not a proper debuggee value, throw a
TypeError instead.

### `adoptFrame(frame)`
Given `frame` of type `Debugger.Frame` which is owned by an arbitrary
`Debugger`, return an equivalent `Debugger.Frame` owned by this `Debugger`.
If the `frame` is associated with a debuggee that is _not_ a debuggee of
the adopting debugger, this method will throw.

### `adoptSource(source)`
Given `source` of type `Debugger.Source` which is owned by an arbitrary
`Debugger`, return an equivalent `Debugger.Source` owned by this `Debugger`.

## Static methods of the Debugger Object

The functions described below are not called with a `this` value.

### `isCompilableUnit(source)`
:   Given a string of source code, designated by <i>source</i>, return false if
    the string might become a valid JavaScript statement with the addition of
    more lines. Otherwise return true. The intent is to support interactive
    compilation - accumulate lines in a buffer until isCompilableUnit is true,
    then pass it to the compiler.

[add]: #adddebuggee-global
[source]: Debugger.Source.md
[script]: Debugger.Script.md
[rv]: Conventions.html#resumption-values
[object]: Debugger.Object.md
[vf]: Debugger.Frame.html#visible-frames
[tracking-allocs]: Debugger.Memory.html#trackingallocationsites
[frame]: Debugger.Frame.md
