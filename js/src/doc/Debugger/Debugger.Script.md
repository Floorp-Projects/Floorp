# Debugger.Script

A `Debugger.Script` instance may refer to a sequence of bytecode in the
debuggee or to a block of WebAssembly code. For the former, it is the
[`Debugger`][debugger-object] API's presentation of a JSAPI `JSScript`
object. The two cases are distinguished by their `format` property being
`"js"` or `"wasm"`.

## Debugger.Script for JSScripts

For `Debugger.Script` instances referring to a `JSScript`, they are
distinguished by their `format` property being `"js"`.

Each of the following is represented by a single `JSScript` object:

* The body of a function—that is, all the code in the function that is not
  contained within some nested function.

* The code passed to a single call to `eval`, excluding the bodies of any
  functions that code defines.

* The contents of a `<script>` element.

* A DOM event handler, whether embedded in HTML or attached to the element
  by other JavaScript code.

* Code appearing in a `javascript:` URL.

The [`Debugger`][debugger-object] interface constructs `Debugger.Script` objects as scripts
of debuggee code are uncovered by the debugger: via the `onNewScript`
handler method; via [`Debugger.Frame`][frame]'s `script` properties; via the
`functionScript` method of [`Debugger.Object`][object] instances; and so on. For a
given [`Debugger`][debugger-object] instance, SpiderMonkey constructs exactly one
`Debugger.Script` instance for each underlying script object; debugger
code can add its own properties to a script object and expect to find
them later, use `==` to decide whether two expressions refer to the same
script, and so on.

(If more than one [`Debugger`][debugger-object] instance is debugging the same code, each
[`Debugger`][debugger-object] gets a separate `Debugger.Script` instance for a given
script. This allows the code using each [`Debugger`][debugger-object] instance to place
whatever properties it likes on its `Debugger.Script` instances, without
worrying about interfering with other debuggers.)

A `Debugger.Script` instance is a strong reference to a JSScript object;
it protects the script it refers to from being garbage collected.

Note that SpiderMonkey may use the same `Debugger.Script` instances for
equivalent functions or evaluated code—that is, scripts representing the
same source code, at the same position in the same source file,
evaluated in the same lexical environment.

## Debugger.Script for WebAssembly

For `Debugger.Script` instances referring to a block of WebAssembly code, they
are distinguished by their `format` property being `"wasm"`.

Currently only entire modules evaluated via `new WebAssembly.Module` are
represented.

`Debugger.Script` objects for WebAssembly are uncovered via `onNewScript` when
a new WebAssembly module is instantiated and via the `findScripts` method on
[`Debugger`][debugger-object] instances. SpiderMonkey constructs exactly one
`Debugger.Script` for each underlying WebAssembly module per
[`Debugger`][debugger-object] instance.

A `Debugger.Script` instance is a strong reference to the underlying
WebAssembly module; it protects the module it refers to from being garbage
collected.

Please note at the time of this writing, support for WebAssembly is
very preliminary. Many properties and methods below throw.

## Convention

For descriptions of properties and methods below, if the behavior of the
property or method differs between the instance referring to a `JSScript` or
to a block of WebAssembly code, the text will be split into two sections,
headed by "**if the instance refers to a `JSScript`**" and "**if the instance
refers to WebAssembly code**", respectively. If the behavior does not differ,
no such emphasized headings will appear.

## Accessor Properties of the Debugger.Script Prototype Object

A `Debugger.Script` instance inherits the following accessor properties
from its prototype:

### `isGeneratorFunction`
True if this instance refers to a `JSScript` for a function defined with a
`function*` expression or statement. False otherwise.

### `isAsyncFunction`
True if this instance refers to a `JSScript` for an async function, defined
with an `async function` expression or statement. False otherwise.

### `isFunction`
True if this instance refers to a `JSScript` for a function. False
otherwise.

### `isModule`
True if this instance refers to a `JSScript` that was parsed and loaded
as an ECMAScript module. False otherwise.

### `displayName`
**If the instance refers to a `JSScript`**, this is the script's display
name, if it has one. If the script has no display name &mdash; for example,
if it is a top-level `eval` script &mdash; this is `undefined`.

If the script's function has a given name, its display name is the same as
its function's given name.

If the script's function has no name, SpiderMonkey attempts to infer an
appropriate name for it given its context. For example:

```js
function f() {}          // display name: f (the given name)
var g = function () {};  // display name: g
o.p = function () {};    // display name: o.p
var q = {
  r: function () {}      // display name: q.r
};
```

Note that the display name may not be a proper JavaScript identifier,
or even a proper expression: we attempt to find helpful names even when
the function is not immediately assigned as the value of some variable
or property. Thus, we use <code><i>a</i>/<i>b</i></code> to refer to
the <i>b</i> defined within <i>a</i>, and <code><i>a</i>&lt;</code> to
refer to a function that occurs somewhere within an expression that is
assigned to <i>a</i>. For example:

```js
function h() {
  var i = function() {};    // display name: h/i
  f(function () {});        // display name: h/<
}
var s = f(function () {});  // display name: s<
```

**If the instance refers to WebAssembly code**, throw a `TypeError`.

### `parameterNames`
**If the instance refers to a `JSScript`**, the names of its parameters,
as an array of strings. If the script is not a function script this is
`undefined`.

If the function uses destructuring parameters, the corresponding array elements
are `undefined`. For example, if the referent is a function script declared in this
way:

```js
function f(a, [b, c], {d, e:f}) { ... }
```

then this `Debugger.Script` instance's `parameterNames` property would
have the value:

```js
["a", undefined, undefined]
```

**If the instance refers to WebAssembly code**, throw a `TypeError`.

### `url`
**If the instance refers to a `JSScript`**, the filename or URL from which
this script's code was loaded. For scripts created by `eval` or the
`Function` constructor, this may be a synthesized filename, starting with a
valid URL and followed by information tracking how the code was introduced
into the system; the entire string is not a valid URL. For
`Function.prototype`'s script, this is `null`. If this `Debugger.Script`'s
`source` property is non-`null`, then this is equal to `source.url`.

**If the instance refers to WebAssembly code**, throw a `TypeError`.

### `startLine`
**If the instance refers to a `JSScript`**, the number of the line at
which this script's code starts, within the file or document named by
`url`.

### `startColumn`
**If the instance refers to a `JSScript`**, the zero-indexed number of the
column at which this script's code starts, within the file or document
named by `url`.  For functions, this is the start of the function's
arguments:
```js
function f() { ... }
//        ^ start (column 10)
let g = x => x*x;
//      ^ start (column 8)
let h = (x) => x*x;
//      ^ start (column 8)
```
For default class constructors, it is the start of the `class` keyword:
```js
let MyClass = class { };
//            ^ start (column 14)
```
For scripts from other sources, such as `eval` or the `Function`
constructor, it is typically 0:
```js
let f = new Function("  console.log('hello world');");
//                    ^ start (column 0, from the string's perspective)
```

### `lineCount`
**If the instance refers to a `JSScript`**, the number of lines this
script's code occupies, within the file or document named by `url`.

### `source`
**If the instance refers to a `JSScript`**, the
[`Debugger.Source`][source] instance representing the source code from
which this script was produced. This is `null` if the source code was not
retained.

**If the instance refers to WebAssembly code**, the
[`Debugger.Source`][source] instance representing the serialized text
format of the WebAssembly code.

### `sourceStart`
**If the instance refers to a `JSScript`**, the character within the
[`Debugger.Source`][source] instance given by `source` at which this
script's code starts; zero-based. If this is a function's script, this is
the index of the start of the `function` token in the source code.

**If the instance refers to WebAssembly code**, throw a `TypeError`.

### `sourceLength`
**If the instance refers to a `JSScript`**, the length, in characters, of
this script's code within the [`Debugger.Source`][source] instance given
by `source`.

**If the instance refers to WebAssembly code**, throw a `TypeError`.

### `mainOffset`
**If the instance refers to a `JSScript`**, the offset of the main
entry point of the script, excluding any prologue.

**If the instance refers to WebAssembly code**, throw a `TypeError`.

### `global`
**If the instance refers to a `JSScript`**, a [`Debugger.Object`][object]
instance referring to the global object in whose scope this script
runs. The result refers to the global directly, not via a wrapper or a
`WindowProxy` ("outer window", in Firefox).

**If the instance refers to WebAssembly code**, throw a `TypeError`.

### `format`
**If the instance refers to a `JSScript`**, `"js"`.

**If the instance refers to WebAssembly code**, `"wasm"`.

## Function Properties of the Debugger.Script Prototype Object

The functions described below may only be called with a `this` value
referring to a `Debugger.Script` instance; they may not be used as
methods of other kinds of objects.

### `getChildScripts()`
**If the instance refers to a `JSScript`**, return a new array whose
elements are Debugger.Script objects for each function
in this script. Only direct children are included; nested
children can be reached by walking the tree.

**If the instance refers to WebAssembly code**, throw a `TypeError`.

### `getPossibleBreakpoints(query)`
Query for the recommended breakpoint locations available in SpiderMonkey.
Returns a result array of objects with the following properties:
 * `offset: number` - The offset the breakpoint.
 * `lineNumber: number` - The line number of the breakpoint.
 * `columnNumber: number` - The column number of the breakpoint.
 * `isStepStart: boolean` - True if SpiderMonkey recommends that the
   breakpoint be treated as a step location when users of debuggers
   step to the next item. This _roughly_ translates to the start of
   each statement, though not entirely.

The `query` argument can be used to filter the set of breakpoints.
The `query` object can contain the following properties:

 * `minOffset: number` - The inclusive lower bound of `offset` values to include.
 * `maxOffset: number` - The exclusive upper bound of `offset` values to include.
 * `line: number` - Limit to breakpoints on the given line.
 * `minLine: number` - The inclusive lower bound of lines to include.
 * `minColumn: number` - The inclusive lower bound of the line/minLine column to include.
 * `maxLine: number` - The exclusive upper bound of lines to include.
 * `maxColumn: number` - The exclusive upper bound of the line/maxLine column to include.

### `getPossibleBreakpointOffsets(query)`
Query for the recommended breakpoint locations available in SpiderMonkey.
Identical to getPossibleBreakpoints except this returns an array of `offset`
values instead of offset metadata objects.

### `getOffsetMetadata(offset)`
Get metadata about a given bytecode offset.
Returns an object with the following properties:
  * `lineNumber: number` - The line number of the breakpoint.
  * `columnNumber: number` - The column number of the breakpoint.
  * `isBreakpoint: boolean` - True if this offset qualifies as a breakpoint,
    defined using the same semantics used for `getPossibleBreakpoints()`.
  * `isStepStart: boolean` - True if SpiderMonkey recommends that the
    breakpoint be treated as a step location when users of debuggers
    step to the next item. This _roughly_ translates to the start of
    each statement, though not entirely.

### `setBreakpoint(offset, handler)`
**If the instance refers to a `JSScript`**, set a breakpoint at the
bytecode instruction at <i>offset</i> in this script, reporting hits to
the `hit` method of <i>handler</i>. If <i>offset</i> is not a valid offset
in this script, throw an error.

When execution reaches the given instruction, SpiderMonkey calls the
`hit` method of <i>handler</i>, passing a [`Debugger.Frame`][frame]
instance representing the currently executing stack frame. The `hit`
method's return value should be a [resumption value][rv], determining
how execution should continue.

Any number of breakpoints may be set at a single location; when control
reaches that point, SpiderMonkey calls their handlers in an unspecified
order.

Any number of breakpoints may use the same <i>handler</i> object.

Breakpoint handler method calls are cross-compartment, intra-thread
calls: the call takes place in the same thread that hit the breakpoint,
and in the compartment containing the handler function (typically the
debugger's compartment).

The new breakpoint belongs to the [`Debugger`][debugger-object] instance to
which this script belongs. Removing a global from the
[`Debugger`][debugger-object] instance's set of debuggees clears all the
breakpoints belonging to that [`Debugger`][debugger-object] instance in that
global's scripts.

### `getBreakpoints([offset])`
**If the instance refers to a `JSScript`**, return an array containing the
handler objects for all the breakpoints set at <i>offset</i> in this
script. If <i>offset</i> is omitted, return the handlers of all
breakpoints set anywhere in this script. If <i>offset</i> is present, but
not a valid offset in this script, throw an error.

**If the instance refers to WebAssembly code**, throw a `TypeError`.

### `clearBreakpoint(handler, [offset])`
**If the instance refers to a `JSScript`**, remove all breakpoints set in
this [`Debugger`][debugger-object] instance that use <i>handler</i> as
their handler. If <i>offset</i> is given, remove only those breakpoints
set at <i>offset</i> that use <i>handler</i>; if <i>offset</i> is not a
valid offset in this script, throw an error.

Note that, if breakpoints using other handler objects are set at the
same location(s) as <i>handler</i>, they remain in place.

### `clearAllBreakpoints([offset])`
**If the instance refers to a `JSScript`**, remove all breakpoints set in
this script. If <i>offset</i> is present, remove all breakpoints set at
that offset in this script; if <i>offset</i> is not a valid bytecode
offset in this script, throw an error.

### `getEffectfulOffsets()`
**If the instance refers to a `JSScript`**, return an array
containing the offsets of all bytecodes in the script which can have direct
side effects that are visible outside the currently executing frame.  This
includes, for example, operations that set properties or elements on
objects, or that may set names in environments created outside the frame.

### `getOffsetsCoverage()`:
**If the instance refers to a `JSScript`**, return `null` or an array which
contains information about the coverage of all opcodes. The elements of
the array are objects, each of which describes a single opcode, and
contains the following properties:

 * `lineNumber`: the line number of the current opcode.

 * `columnNumber`: the column number of the current opcode.

 * `offset`: the bytecode instruction offset of the current opcode.

 * `count`: the number of times the current opcode got executed.

If this script has no coverage, or if it is not instrumented, then this
function will return `null`. To ensure that the debuggee is instrumented,
the flag `Debugger.collectCoverageInfo` should be set to `true`.

**If the instance refers to WebAssembly code**, throw a `TypeError`.

### `isInCatchScope([offset])`
**If the instance refers to a `JSScript`**, this is `true` if this offset
falls within the scope of a try block, and `false` otherwise.

**If the instance refers to WebAssembly code**, throw a `TypeError`.

### Deprecated Debugger.Script Prototype Functions

The following functions have all been deprecated in favor of `getOffsetMetadata`,
`getPossibleBreakpoints`, and `getPossibleBreakpointOffsets`. These functions
all have an under-defined concept of what offsets are and are not included
in their results.

#### `getAllOffsets()`
**If the instance refers to a `JSScript`**, return an array <i>L</i>
describing the relationship between bytecode instruction offsets and
source code positions in this script. <i>L</i> is sparse, and indexed by
source line number. If a source line number <i>line</i> has no code, then
<i>L</i> has no <i>line</i> property. If there is code for <i>line</i>,
then <code><i>L</i>[<i>line</i>]</code> is an array of offsets of byte
code instructions that are entry points to that line.

For example, suppose we have a script for the following source code:

```js
a=[]
for (i=1; i < 10; i++)
    // It's hip to be square.
    a[i] = i*i;
```

Calling `getAllOffsets()` on that code might yield an array like this:

```js
[[0], [5, 20], , [10]]
```

This array indicates that:

* the first line's code starts at offset 0 in the script;

* the `for` statement head has two entry points at offsets 5 and 20 (for
the initialization, which is performed only once, and the loop test,
which is performed at the start of each iteration);

* the third line has no code;

* and the fourth line begins at offset 10.

**If the instance refers to WebAssembly code**, throw a `TypeError`.

#### `getAllColumnOffsets()`
**If the instance refers to a `JSScript`**, return an array describing the
relationship between bytecode instruction offsets and source code
positions in this script. Unlike getAllOffsets(), which returns all
offsets that are entry points for each line, getAllColumnOffsets() returns
all offsets that are entry points for each (line, column) pair.

The elements of the array are objects, each of which describes a single
entry point, and contains the following properties:

* lineNumber: the line number for which offset is an entry point

* columnNumber: the column number for which offset is an entry point

* offset: the bytecode instruction offset of the entry point

For example, suppose we have a script for the following source code:

```js
a=[]
for (i=1; i < 10; i++)
    // It's hip to be square.
    a[i] = i*i;
```

Calling `getAllColumnOffsets()` on that code might yield an array like this:

```js
[{ lineNumber: 0, columnNumber: 0, offset: 0 },
  { lineNumber: 1, columnNumber: 5, offset: 5 },
  { lineNumber: 1, columnNumber: 10, offset: 20 },
  { lineNumber: 3, columnNumber: 4, offset: 10 }]
```

**If the instance refers to WebAssembly code**, throw a `TypeError`.

#### `getLineOffsets(line)`
**If the instance refers to a `JSScript`**, return an array of bytecode
instruction offsets representing the entry points to source line
<i>line</i>. If the script contains no executable code at that line, the
array returned is empty.

#### `getOffsetLocation(offset)`
**If the instance refers to a `JSScript`**, return an object describing the
source code location responsible for the bytecode at <i>offset</i> in this
script.  The object has the following properties:

* `lineNumber`: the line number for which offset is an entry point

* `columnNumber`: the column number for which offset is an entry point

* `isEntryPoint`: true if the offset is a column entry point, as
  would be reported by getAllColumnOffsets(); otherwise false.


[debugger-object]: Debugger.md
[source]: Debugger.Source.md
[object]: Debugger.Object.md
[frame]: Debugger.Frame.md
[rv]: ./Conventions.html#resumption-values
