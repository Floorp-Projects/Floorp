# Debugger.Environment

A `Debugger.Environment` instance represents a lexical environment,
associating names with variables. Each [`Debugger.Frame`][frame] instance
representing a debuggee frame has an associated environment object
describing the variables in scope in that frame; and each
[`Debugger.Object`][object] instance representing a debuggee function has an
environment object representing the environment the function has closed
over.

ECMAScript environments form a tree, in which each local environment is
parented by its enclosing environment (in ECMAScript terms, its 'outer'
environment). We say an environment <i>binds</i> an identifier if that
environment itself associates the identifier with a variable, independently
of its outer environments. We say an identifier is <i>in scope</i> in an
environment if the identifier is bound in that environment or any enclosing
environment.

SpiderMonkey creates `Debugger.Environment` instances as needed as the
debugger inspects stack frames and function objects; calling
`Debugger.Environment` as a function or constructor raises a `TypeError`
exception.

SpiderMonkey creates exactly one `Debugger.Environment` instance for each
environment it presents via a given [`Debugger`][debugger-object] instance:
if the debugger encounters the same environment through two different
routes (perhaps two functions have closed over the same environment),
SpiderMonkey presents the same `Debugger.Environment` instance to the
debugger each time. This means that the debugger can use the `==` operator
to recognize when two `Debugger.Environment` instances refer to the same
environment in the debuggee, and place its own properties on a
`Debugger.Environment` instance to store metadata about particular
environments.

(If more than one [`Debugger`][debugger-object] instance is debugging the
same code, each [`Debugger`][debugger-object] gets a separate
`Debugger.Environment` instance for a given environment. This allows the
code using each [`Debugger`][debugger-object] instance to place whatever
properties it likes on its own [`Debugger.Object`][object] instances,
without worrying about interfering with other debuggers.)

If a `Debugger.Environment` instance's referent is not a debuggee
environment, then attempting to access its properties (other than
`inspectable`) or call any its methods throws an instance of `Error`.

`Debugger.Environment` instances protect their referents from the
garbage collector; as long as the `Debugger.Environment` instance is
live, the referent remains live. Garbage collection has no visible
effect on `Debugger.Environment` instances.


## Accessor Properties of the Debugger.Environment Prototype Object

A `Debugger.Environment` instance inherits the following accessor
properties from its prototype:

### `inspectable`
True if this environment is a debuggee environment, and can therefore
be inspected. False otherwise. All other properties and methods of
`Debugger.Environment` instances throw if applied to a non-inspectable
environment.

### `type`
The type of this environment object, one of the following values:

* "declarative", indicating that the environment is a declarative
  environment record. Function calls, calls to `eval`, `let` blocks,
  `catch` blocks, and the like create declarative environment records.

* "object", indicating that the environment's bindings are the
  properties of an object. The global object and DOM elements appear in
  the chain of environments via object environments. (Note that `with`
  statements have their own environment type.)

* "with", indicating that the environment was introduced by a `with`
  statement.

### `scopeKind`
If this is a declarative environment, a string describing the kind of scope
which this environment is associated with, or `null` for other types of
environments.  There is an assortment of possible scope kinds which can be
generated, with a selection of possible values below.  Unlike the type
accessor, the categorization this performs is specific to SpiderMonkey's
implementation, and not derived from distinctions made in the ECMAScript
language specification.

* "function", indicating the top level body scope of a function for
arguments and 'var' variables.

* "function lexical", indicating the top level lexical scope in a function.

### `parent`
The environment that encloses this one (the "outer" environment, in
ECMAScript terminology), or `null` if this is the outermost environment.

### `object`
A [`Debugger.Object`][object] instance referring to the object whose
properties this environment reflects. If this is a declarative
environment record, this accessor throws a `TypeError` (since
declarative environment records have no such object). Both `"object"`
and `"with"` environments have `object` properties that provide the
object whose properties they reflect as variable bindings.

### `calleeScript`
If this environment represents the variable environment (the top-level
environment within the function, which receives `var` definitions) for
a call to a function <i>f</i>, then this property's value is a
[`Debugger.Script`][script] instance referring to <i>f</i>'s script. Otherwise,
this property's value is `null`.

### `optimizedOut`
True if this environment is optimized out. False otherwise. For example,
functions whose locals are never aliased may present optimized-out
environments. When true, `getVariable` returns an ordinary JavaScript
object whose `optimizedOut` property is true on all bindings, and
`setVariable` throws a `ReferenceError`.


## Function Properties of the Debugger.Environment Prototype Object

The methods described below may only be called with a `this` value
referring to a `Debugger.Environment` instance; they may not be used as
methods of other kinds of objects.

### `names()`
Return an array of strings giving the names of the identifiers bound by
this environment. The result does not include the names of identifiers
bound by enclosing environments.

### `getVariable(name)`

Return the value of the variable bound to <i>name</i> in this
environment, or `undefined` if this environment does not bind
<i>name</i>. <i>Name</i> must be a string that is a valid ECMAScript
identifier name. The result is a debuggee value, in most cases.

JavaScript engines often omit variables from environments, to save space
and reduce execution time. If the given variable should be in scope, but
`getVariable` is unable to produce its value, it returns an ordinary
JavaScript object (not a [`Debugger.Object`][object] instance) whose
`optimizedOut` property is `true`.

Aside from the above case, this method can return something that is not a
debuggee value in two other cases. If a function argument is missing, then it
returns an ordinary JavaScript object whose `missingArgument` property is
`true`. Finally, if a variable name is bound in the environment but not yet
initialized (for example, if the debuggee is paused in the middle of an
initializer expression) then it returns an ordinary JavaScript object whose
`uninitialized` property is `true`.

This is not an [invocation function][inv fr];
if this call would cause debuggee code to run (say, because the
environment is a `"with"` environment, and <i>name</i> refers to an
accessor property of the `with` statement's operand), this call throws a
[`Debugger.DebuggeeWouldRun`][wouldrun]
exception.

### `setVariable(name, value)`
Store <i>value</i> as the value of the variable bound to <i>name</i> in
this environment. <i>Name</i> must be a string that is a valid
ECMAScript identifier name; <i>value</i> must be a debuggee value.

If this environment binds no variable named <i>name</i>, throw a
`ReferenceError`.

This is not an [invocation function][inv fr];
if this call would cause debuggee code to run, this call throws a
[`Debugger.DebuggeeWouldRun`][wouldrun]
exception.

### `find(name)`
Return a reference to the innermost environment, starting with this
environment, that binds <i>name</i>. If <i>name</i> is not in scope in
this environment, return `null`. <i>Name</i> must be a string whose
value is a valid ECMAScript identifier name.


[frame]: Debugger.Frame.md
[object]: Debugger.Object.md
[debugger-object]: Debugger.md
[inv fr]: Debugger.Frame.html#invocation-functions-and-debugger-frames
[wouldrun]: Conventions.html#the-debugger-debuggeewouldrun-exception
