# Debugger.Frame

A `Debugger.Frame` instance represents a [visible stack frame][vf]. Given a
`Debugger.Frame` instance, you can find the script the frame is executing,
walk the stack to older frames, find the lexical environment in which the
execution is taking place, and so on.

For a given [`Debugger`][debugger-object] instance, SpiderMonkey creates
only one `Debugger.Frame` instance for a given visible frame. Every handler
method called while the debuggee is running in a given frame is given the
same frame object. Similarly, walking the stack back to a previously
accessed frame yields the same frame object as before. Debugger code can
add its own properties to a frame object and expect to find them later, use
`==` to decide whether two expressions refer to the same frame, and so on.

(If more than one [`Debugger`][debugger-object] instance is debugging the
same code, each [`Debugger`][debugger-object] gets a separate
`Debugger.Frame` instance for a given frame. This allows the code using
each [`Debugger`][debugger-object] instance to place whatever properties it
likes on its `Debugger.Frame` instances, without worrying about interfering
with other debuggers.)

When the debuggee pops a stack frame (say, because a function call has
returned or an exception has been thrown from it), the `Debugger.Frame`
instance referring to that frame becomes inactive: its `onStack` property
becomes `false`, and accessing its many properties or calling its methods
throws an exception. Note that frames only become inactive at times that
are predictable for the debugger: when the debuggee runs, or when the
debugger removes frames from the stack itself.


## Visible Frames

When inspecting the call stack, [`Debugger`][debugger-object] does not
reveal all the frames that are actually present on the stack: while it does
reveal all frames running debuggee code, it omits frames running the
debugger's own code, and omits most frames running non-debuggee code. We
call those stack frames a [`Debugger`][debugger-object] does reveal
<i>visible frames</i>.

A frame is a visible frame if any of the following are true:

* it is running [debuggee code][dbg code];

* its immediate caller is a frame running debuggee code; or

* it is a [`"debugger"` frame][inv fr],
  representing the continuation of debuggee code invoked by the debugger.

The "immediate caller" rule means that, when debuggee code calls a
non-debuggee function, it looks like a call to a primitive: you see a frame
for the non-debuggee function that was accessible to the debuggee, but any
further calls that function makes are treated as internal details, and
omitted from the stack trace. If the non-debuggee function eventually calls
back into debuggee code, then those frames are visible.

(Note that the debuggee is not considered an "immediate caller" of handler
methods it triggers. Even though the debuggee and debugger share the same
JavaScript stack, frames pushed for SpiderMonkey's calls to handler methods
to report events in the debuggee are never considered visible frames.)


## Invocation Functions and "debugger" Frames

An <i>invocation function</i> is any function in this interface that allows
the debugger to invoke code in the debuggee:
`Debugger.Object.prototype.call`, `Debugger.Frame.prototype.eval`, and so
on.

While invocation functions differ in the code to be run and how to pass
values to it, they all follow this general procedure:

1. Let <i>older</i> be the youngest visible frame on the stack, or `null`
   if there is no such frame. (This is never one of the the debugger's own
   frames; those never appear as `Debugger.Frame` instances.)

2. Push a `"debugger"` frame on the stack, with <i>older</i> as its
   `older` property.

3. Invoke the debuggee code as appropriate for the given invocation
   function, with the `"debugger"` frame as its continuation. For example,
   `Debugger.Frame.prototype.eval` pushes an `"eval"` frame for code it
   runs, whereas `Debugger.Object.prototype.call` pushes a `"call"` frame.

4. When the debuggee code completes, whether by returning, throwing an
   exception or being terminated, pop the `"debugger"` frame, and return an
   appropriate [completion value][cv] from the invocation function to the
   debugger.

When a debugger calls an invocation function to run debuggee code, that
code's continuation is the debugger, not the next debuggee code frame.
Pushing a `"debugger"` frame makes this continuation explicit, and makes it
easier to find the extent of the stack created for the invocation.


## Suspended Frames

Some frames can be *suspended*.

When a generator `yield`s a value, or when an async function `await`s a
value, the current frame is suspended and removed from the stack, and
other JS code has a chance to run. Later (if the `await`ed promise
becomes resolved, for example), SpiderMonkey will *resume* the frame. It
will be put back onto the stack, and execution will continue where it
left off. Only generator and async function call frames can be suspended
and resumed.

Currently, a frame's `onStack` property is `false` while it's suspended
([bug 1448880](https://bugzilla.mozilla.org/show_bug.cgi?id=1448880)).

SpiderMonkey uses the same `Debugger.Frame` object each time a generator
or async function call is put back onto the stack. This means that the
`onStep` handler can be used to step over `yield` and `await`.

The `frame.onPop` handler is called each time a frame is suspended, and
the `Debugger.onEnterFrame` handler is called each time a frame is
resumed. (This means these events can fire multiple times for the same
`Frame` object, which is odd, but accurately conveys what's happening.)

The [completion value][cv] passed to the `frame.onPop` handler for a suspension
contains additional properties to clarify what's going on. See the documentation
for completion values for details.


## Stepping Into Generators: The "Initial Yield"

When a debuggee generator is called, something weird happens. The
`.onEnterFrame` hook fires, as though we're stepping into the generator.
But the code inside the generator doesn't run. Instead it immediately
returns. Then we sometimes get *another* `.onEnterFrame` event for the
same generator. What's going on?

To explain this, we first have to describe how generator calls work,
according to the ECMAScript language specification. Note that except for
step 3, it's exactly like a regular function call.

1.  An "execution context" (what we call a `Frame`) is pushed to the stack.
2.  An environment is created (for arguments and local variables).
    Argument-default-value-expressions, if any, are evaluated.
3.  A generator object is created, initially suspended at the start of the generator body.
4.  The stack frame is popped, and the generator object is returned to the caller.

The JavaScript engine actually carries out these steps, in this order.
So when a debuggee generator is called, here's what you'll observe:

1.  The `debugger.onEnterFrame` hook fires.
2.  The debugger can step through the argument-default-value code, if any.
3.  The body of the generator does not run yet. Instead, a generator object
    is created and suspended (which does not fire any debugger events).
4.  The `frame.onPop` hook fires, with a completion value of
    `{return:` *(the new generator object)* `}`.

In SpiderMonkey, this process of suspending and returning a new
generator object is called the "initial yield".

If the caller then uses the generator's `.next()` method, which may or
may not happen right away depending on the debuggee code, the suspended
generator will be resumed, firing `.onEnterFrame` again.

## Accessor Properties of the Debugger.Frame Prototype Object

A `Debugger.Frame` instance inherits the following accessor properties from
its prototype:

### `type`

A string describing what sort of frame this is:

* `"call"`: a frame running a function call. (We may not be able to obtain
  frames for calls to host functions.)

* `"eval"`: a frame running code passed to `eval`.

* `"global"`: a frame running global code (JavaScript that is neither of
  the above).

* `"module"`: a frame running code at the top level of a module.

* `"wasmcall"`: a frame running a WebAssembly function call.

* `"debugger"`: a frame for a call to user code invoked by the debugger
  (see the `eval` method below).

Accessing this property will throw if `.terminated == true`.

### `implementation`
A string describing which tier of the JavaScript engine this frame is
executing in:

* `"interpreter"`: a frame running in the interpreter.

* `"baseline"`: a frame running in the unoptimizing, baseline JIT.

* `"ion"`: a frame running in the optimizing JIT.

* `"wasm"`: a frame running in WebAssembly baseline JIT.

Accessing this property will throw if `.onStack == false`.

### `this`
The value of `this` for this frame (a debuggee value). For a `wasmcall`
frame, this property throws a `TypeError`.

Accessing this property will throw if `.terminated == true`.

### `older`
The `Debugger.Frame` for the next-older visible frame, in which control will
resume when this frame completes. If there is no older frame, this is `null`.
If there an explicitly inserted asynchronous stack trace above this frame,
this is `null`, since the explicit saved frame takes priority.
If this frame is a suspended generator or async call, this will also be `null`.

Accessing this property will throw if `.terminated == true`.

### `olderSavedFrame`

If this frame has no `older` frame, this field may hold a [`SavedFrame`][saved-frame]
object representing the saved asynchronous stack that triggered execution of
this `Debugger.Frame` instance.

Accessing this property will throw if `.terminated == true`.

### `onStack`
True if the frame this `Debugger.Frame` instance refers to is still on
the stack; false if it has completed execution or been popped in some other way.
Note that this property may be accessed regardless of what state the frame is
in, so it can be used to verify whether it is safe to access other properties
that require an on-stack frame.

### `terminated`
True if the frame this `Debugger.Frame` instance refers to will never run
again; false if it is on-stack or is a suspended generator/async call that
may be resumed later. Note that this property may be accessed regardless of
what state the frame is in, so it can be used to verify whether it is safe to
access other properties that require a non-terminated frame.

### `script`
The script being executed in this frame (a [`Debugger.Script`][script]
instance), or `null` on frames that do not represent calls to debuggee
code. On frames whose `callee` property is not null, this is equal to
`callee.script`.

Accessing this property will throw if `.terminated == true`.

### `offset`
The offset of the bytecode instruction currently being executed in
`script`, or `undefined` if the frame's `script` property is `null`.
For a `wasmcall` frame, this property throws a `TypeError`.

If this is used on a suspended function frame, the offset will reference
the offset where the frame will be resumed.

Accessing this property will throw if `.terminated == true`.

### `environment`
The lexical environment within which evaluation is taking place (a
[`Debugger.Environment`][environment] instance), or `null` on frames
that do not represent the evaluation of debuggee code, like calls
non-debuggee functions, host functions or `"debugger"` frames.

Accessing this property will throw if `.terminated == true`.

### `callee`
The function whose application created this frame, as a debuggee value,
or `null` if this is not a `"call"` frame.

Accessing this property will throw if `.terminated == true`.

### `constructing`
True if this frame is for a function called as a constructor, false
otherwise.

Accessing this property will throw if `.terminated == true`.

### `arguments`
The arguments passed to the current frame, or `null` if this is not a
`"call"` frame. When non-`null`, this is an object, allocated in the
same global as the debugger, with `Array.prototype` on its prototype
chain, a non-writable `length` property, and properties whose names are
array indices. Each property is a read-only accessor property whose
getter returns the current value of the corresponding parameter. When
the referent frame is popped, the argument value's properties' getters
throw an error.

Accessing this property will throw if `.onStack == false`.

### `asyncPromise`

If the frame is not an async (generator) function, this will be `undefined`.

For async functions, this will be a [`Debugger.Object`][object] whose referent
is the promise for async function call's return value. Note that this
property will be `null` if the value is accessed during `onEnterFrame`,
since the promise doesn't exist yet at that point.

For async generator functions, this will be a [`Debugger.Object`][object]
whose referent is the promise for the current iteration's "value"+"done" object,
which will be resolved when the generator next throws/yields/returns.
Note that this will be `null` if the value is accessed during the initial
generator `onEnterFrame`/`onPop` (before the first `.next` call),
since there is no promise yet at that point.

Accessing this property will throw if `.terminated == true`.


## Handler Methods of Debugger.Frame Instances

Each `Debugger.Frame` instance inherits accessor properties holding handler
functions for SpiderMonkey to call when given events occur in the frame.

Calls to frames' handler methods are cross-compartment, intra-thread calls:
the call takes place in the thread to which the frame belongs, and runs in
the compartment to which the handler method belongs.

`Debugger.Frame` instances inherit the following handler method properties:

### `onStep`
This property must be either `undefined` or a function. If it is a
function, SpiderMonkey calls it when execution in this frame makes a
small amount of progress, passing no arguments and providing this
`Debugger.Frame` instance as the `this`value. The function should
return a [resumption value][rv] specifying how the debuggee's execution
should proceed.

What constitutes "a small amount of progress" varies depending on the
implementation, but it is fine-grained enough to implement useful
"step" and "next" behavior.

If multiple [`Debugger`][debugger-object] instances each have
`Debugger.Frame` instances for a given stack frame with `onStep`
handlers set, their handlers are run in an unspecified order. If any
`onStep` handler forces the frame to return early (by returning a
resumption value other than `undefined`), any remaining debuggers'
`onStep` handlers do not run.

This property is ignored on frames that are not executing debuggee
code, like `"call"` frames for calls to host functions and `"debugger"`
frames.

Accessing and reassigning this property is allowed independent of whether
or not the frame is currently on-stack/suspended/terminated.

### `onPop`
This property must be either `undefined` or a function. If it is a function,
SpiderMonkey calls it just before this frame is popped or suspended, passing
a [completion value][cv] indicating the reason, and providing this
`Debugger.Frame` instance as the `this` value. The function should return a
[resumption value][rv] indicating how execution should proceed. On newly
created frames, this property's value is `undefined`.

When this handler is called, this frame's current execution location, as
reflected in its `offset` and `environment` properties, is the operation
which caused it to be unwound. In frames returning or throwing an exception,
the location is often a return or a throw statement. In frames propagating
exceptions, the location is a call. In generator or async function frames,
the location may be a `yield` or `await` expression.

When an `onPop` call reports the completion of a construction call
(that is, a function called via the `new` operator), the completion
value passed to the handler describes the value returned by the
function body. If this value is not an object, it may be different from
the value produced by the `new` expression, which will be the value of
the frame's `this` property. (In ECMAScript terms, the `onPop` handler
receives the value returned by the `[[Call]]` method, not the value
returned by the `[[Construct]]` method.)

When a debugger handler function forces a frame to complete early, by
returning a `{ return:... }`, `{ throw:... }`, or `null` resumption
value, SpiderMonkey calls the frame's `onPop` handler, if any. The
completion value passed in this case reflects the resumption value that
caused the frame to complete.

When SpiderMonkey calls an `onPop` handler for a frame that is throwing
an exception or being terminated, and the handler returns `undefined`,
then SpiderMonkey proceeds with the exception or termination. That is,
an `undefined` resumption value leaves the frame's throwing and
termination process undisturbed.

If multiple [`Debugger`][debugger-object] instances each have
`Debugger.Frame` instances for a given stack frame with `onPop`
handlers set, their handlers are run in an unspecified order. The
resumption value each handler returns establishes the completion value
reported to the next handler.

The `onPop` handler is typically called only once for a given
`Debugger.Frame`, after which the frame becomes inactive. However, in the
case of [generators and async functions](#suspended-frames), `onPop` fires
each time the frame is suspended.

This handler is not called on `"debugger"` frames. It is also not called
when unwinding a frame due to an over-recursion or out-of-memory
exception.

Accessing and reassigning this property is allowed independent of whether
or not the frame is currently on-stack/suspended/terminated.


## Function Properties of the Debugger.Frame Prototype Object

The functions described below may only be called with a `this` value
referring to a `Debugger.Frame` instance; they may not be used as
methods of other kinds of objects.

### `eval(code, [options])`
Evaluate <i>code</i> in the execution context of this frame, and return
a [completion value][cv] describing how it completed. <i>Code</i> is a
string. If this frame's `environment` property is `null` or `type` property
is `wasmcall`, throw a `TypeError`. All extant handler methods, breakpoints,
and so on remain active during the call. This function follows the
[invocation function conventions][inv fr].

<i>Code</i> is interpreted as strict mode code when it contains a Use
Strict Directive, or the code executing in this frame is strict mode
code.

If <i>code</i> is not strict mode code, then variable declarations in
<i>code</i> affect the environment of this frame. (In the terms used by
the ECMAScript specification, the `VariableEnvironment` of the
execution context for the eval code is the `VariableEnvironment` of the
execution context that this frame represents.) If implementation
restrictions prevent SpiderMonkey from extending this frame's
environment as requested, this call throws an Error exception.

If given, <i>options</i> should be an object whose properties specify
details of how the evaluation should occur. The `eval` method
recognizes the following properties:

* `url`

  The filename or URL to which we should attribute <i>code</i>. If this
  property is omitted, the URL defaults to `"debugger eval code"`.

* `lineNumber`

  The line number at which the evaluated code should be claimed to begin
  within <i>url</i>.

Accessing this property will throw if `.onStack == false`.

### `evalWithBindings(code, bindings, [options])`
Like `eval`, but evaluate <i>code</i> in the environment of this frame,
extended with bindings from the object <i>bindings</i>. For each own
enumerable property of <i>bindings</i> named <i>name</i> whose value is
<i>value</i>, include a variable in the environment in which
<i>code</i> is evaluated named <i>name</i>, whose value is
<i>value</i>. Each <i>value</i> must be a debuggee value. (This is not
like a `with` statement: <i>code</i> may access, assign to, and delete
the introduced bindings without having any effect on the
<i>bindings</i> object.)

This method allows debugger code to introduce temporary bindings that
are visible to the given debuggee code and which refer to debugger-held
debuggee values, and do so without mutating any existing debuggee
environment.

Note that, like `eval`, declarations in the <i>code</i> passed to
`evalWithBindings` affect the environment of this frame, even as that
environment is extended by bindings visible within <i>code</i>. (In the
terms used by the ECMAScript specification, the `VariableEnvironment`
of the execution context for the eval code is the `VariableEnvironment`
of the execution context that this frame represents, and the
<i>bindings</i> appear in a new declarative environment, which is the
eval code's `LexicalEnvironment`.) If implementation restrictions
prevent SpiderMonkey from extending this frame's environment as
requested, this call throws an `Error` exception.

The <i>options</i> argument is as for
[`Debugger.Frame.prototype.eval`][fr eval], described above.
Also like `eval`, if this frame's `environment` property is `null` or
`type` property is `wasmcall`, throw a `TypeError`.

Note: If this method is called on an object whose owner
[Debugger object][debugger-object] has an onNativeCall handler, only hooks
on objects associated with that debugger will be called during the evaluation.

Accessing this property will throw if `.onStack == false`.


[vf]: #visible-frames
[debugger-object]: Debugger.md
[object]: Debugger.Object.md
[dbg code]: Conventions.html#debuggee-code
[inv fr]: #invocation-functions-and-debugger-frames
[cv]: Conventions.html#completion-values
[script]: Debugger.Script.md
[environment]: Debugger.Environment.md
[rv]: Conventions.html#resumption-values
[fr eval]: #eval-code-options
[saved-frame]: ../SavedFrame/index
