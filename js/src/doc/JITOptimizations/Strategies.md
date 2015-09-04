# JIT Optimization Strategies

SpiderMonkey's optimizing JIT, IonMonkey, uses a number of different
optimization strategies to speed up various operations. The most commonplace
operations that are relevant for fast program execution are property accesses
and function calls.

Optimization information is currently collected for the following operations:

- [GetProperty](#getprop) (`obj.prop`)
- [SetProperty](#setprop) (`obj.prop = val`)
- [GetElement](#getelem) (`obj[elemName]`)
- [SetElement](#setelem) (`obj[elemName] = val`)
- [Call](#call) (`func(...)`)

At each operation site, IonMonkey tries a battery of <i>strategies</i>, from
the most optimized but most restrictive to the least optimized but least
restrictive. For each strategy attempted, its <i>outcome</i> is tracked. An
outcome is either success or a reason why the strategy failed to apply.

This page documents the various optimization strategies and their outcomes. It
provides information on what they attempt to do, what general level of
speed-up they provide, what kind of program characteristics can prevent them
from being used, and common ways to enable the engine to utilize that
optimization.

## <a name="getprop"></a>GetProperty

### GetProperty_ArgumentsLength

Attempts to optimize an `arguments.length` property access. This optimization
only works if the arguments object is used in well-understood ways within the
function. The function containing the arguments.length is allowed to use the
arguments object in the following ways without disabling this optimization:

- Access `arguments.length`
- Access `arguments.callee`
- Access individual args using `arguments[i]`
- Save arguments into variables, as long as those variables cannot be accessed
  by any nested function, and as long as there exists no eval nywhere within
  the function or nested function definitions.
- Call a function using `f.apply(obj, arguments)`

If the function contains any use of the arguments object that falls out of the
cases defined above, this optimization will be suppressed. In particular,
`arguments` cannot be returned from the function, or passed as an argument into
calls (except for the `apply` case above).

### GetProp_ArgumentsCallee

Attempts to optimize an `arguments.callee` property access. This optimization
only works if the arguments object is used in well-understood ways within the
function. The function containing the `arguments.callee` is allowed to use the
arguments object in the following ways without disabling this optimization:

- Access arguments.length
- Access arguments.callee
- Access individual args using arguments[i]
- Save arguments into variables, as long as those variables cannot be accessed
  by any nested function, and as long as there exists no eval nywhere within
  the function or nested function definitions.
- Call a function using `f.apply(obj, arguments)`

If the function contains any use of the `arguments` object that falls out of
the cases defined above, this optimization will be suppressed. In particular,
arguments cannot be returned from the function, or passed as an argument into
calls (except for the `apply` example listed above).

### GetProp_InferredConstant

Attempts to optimize an access to a property that seems to be a constant. It
applies to property accesses on objects which are global-like in that there is
only one instance of them per program. This includes global objects, object
literals defined at the top-level of a script, and top-level function objects.

This optimization makes the assumption that a property that has not changed
after it was first assigned, is likely a constant property.  It then directly
inlines the value of the property into hot code that accesses it. For
example, in the following code:

    var Constants = {};
    Constants.N = 100;

    function testArray(array) {
      for (var i = 0; i < array.length; i++) {
        if (array[i] > Constants.N)
          return true;
      }
      return false;
    }

Will have the loop compiled into the following when `testArray` gets hot.

    for (var i = 0; i < array.length; i++) {
      if (array[i] > 100)
        return true;
    }

When this optimization is successful, property access is eliminated entirely
and replaced with an inline constant.

### GetProp_Constant

Attempts to optimize reading a property that contains a uniquely-typed (or
"singleton") object.  With uniquely-typed objects, it is guaranteed that
no other object has that same type.  Unique (or "singleton") types are
assigned to certain kinds of objects, like global objects, top-level
functions, and object literals declared at the top-level of a script.  If
a property has always contained the same uniquely-typed object, then the
engine can use the unique type to map back to a specific object, and
eliminate the property access, replacing it with a constant reference to
the object.

When this optimization is successful, property access is eliminated entirely
and replaced with an inline constant. The different success and failure
conditions are documented below:

### GetProp_StaticName

Attempts to optimize a property access on `window` which refers to
a property on the global object.

### GetProp_SimdGetter

Optimizes property accesses to SIMD vectors.  Accesses to properties
signMask, x, y, w, and z are optimized.

### GetProp_TypedObject

Optimizes accesses to properties on TypedObjects.

### GetProp_DefiniteSlot

Optimizes access to a well-known regular property on an object.  For this
optimization to succeed, the property needs to be well-defined on the object.
For objects constructed by constructor functions, this means that the property
needs to be defined in the constructor, before any complex logic occurs within
the constructor.

This is the best case for a regular "field" type property that is not
turned into a constant.  It compiles down to a single CPU-level load
instruction.

    function SomeConstructor() {
        this.x = 10;    // x is a definite slot property
        this.y = 10;    // y is a definite slot property
        someComplicatedFunctionCall();
        this.z = 20;    // z is not a definite slot property.
    }

In the above example, the properties `x` and `y` can be determined to always
exist on any instance of `SomeConstructor` at definite locations, allowing
the engine to deterministically infer the position of `x` without a shape
guard.

This optimization can fail for a number of reasons.  If the types observed
at the property access are polymorphic (more than one type), this optimization
cannot succeed.  Furthermore, even if the object type is monomorphic, the
optimization will fail if the property being accessed is not a definite
slot as described above.

### GetProp_Unboxed

Similar to `GetProp_DefiniteSlot`.  Unboxed property reads are possible on
properties which satisfy all the characteristics of a definite slot, and
additionally have been observed to only store values of one kind of value.

Consider the following constructor:

    function Point(x, y) {
        this.x = x;
        this.y = y;
    }

If only integers are ever stored in the `x` and `y` properties,
then the instances of `Point` will be represented in an "unboxed" mode -
with the property values stored as raw 4-byte values within the object.

Objects which have the unboxed optimization are more compact.

### GetProp_CommonGetter

Optimizes access to properties which are implemented by a getter function,
where the getter is shared between multiple types.

This optimization applies most often when the property access site is
polymorphic, but all the object types are derived variants of a single
base class, where the property access refers to a getter on the base
class.

Consider the following example:
    function Base() {}
    Base.prototype = {
        get x() { return 3; }
    };

    function Derived1() {}
    Derived1.prototype = Object.create(Base.prototype);

    function Derived2() {}
    Derived1.prototype = Object.create(Base.prototype);

If a property access for `d.x` sees only instances of both `Derived1` and
`Derived2` for `d`, it can optimize the access to `x` to a call to the
getter function defined on `Base`.

This optimization applies to shared getters on both pure JS objects as well
as DOM objects.

### GetProp_InlineAccess

Optimizes a polymorphic property access where there are only a few different
types of objects seen, and the property on all of the different types is
determinable through a shape-check.

If a property access is monomorphic and the property's location is determinable
from the object's shape, but the property is not definite (see:
GetProp_DefiniteProperty), then this optimization may be used.

Alternatively, if the property access is polymorphic, but only has a few
different shapes observed at the access site, this optimization may be used.

This optimization compiles down to one-or more shape-guarded direct loads
from the object.  The following pseudocode describes the kind of machine
code generated by this optimization:

    if obj.shape == Shape1 then
      obj.slots[0]
    elif obj.shape == Shape2 then
      obj.slots[5]
    elif obj.shape == Shape3 then
      obj.slots[2]
    ...
    end

### GetProp_Innerize

Attempts to optimize a situation where a property access of the form
`window.PROP` can be directly translated into a property access on
the inner global object.

This optimization will always fail on property accesses which are
not on the window object.

It is useful because accessing global names via the 'window' object
is a common idiom in web programming.

### GetProp_InlineCache

This is the worst-case scenario for a property access optimization.  This
strategy is used when all the others fail.  The engine simply inserts
an inline cache at the property access site.

Inline caches start off as a jump to a separate piece of code called
a "fallback".  The fallback calls into the interpreter VM (which is
very slow) to perform the operation, and then decides if the operation
can be optimized in that particular case.  If so, it generates a new
"stub" (or freestanding piece of jitcode) and changes the inline cache
to jump to the stub.  The stub attempts to optimize further occurrences
of that same kind of operation.

Inline caches are an order of magnitude slower than the other optimization
strategies, and are an indication that the type inference engine has
failed to collect enough information to guide the optimization process.

## <a name="setprop"></a>SetProperty

### SetProp_CommonSetter

Optimizes access to properties which are implemented by a setter function,
where the setter is shared between multiple types.

This optimization applies most often when the property access site is
polymorphic, but all the object types are derived variants of a single
base class, where the property access refers to a setter on the base
class.

Consider the following example:
    function Base() {}
    Base.prototype = {
        set x(val) { ... }
    };

    function Derived1() {}
    Derived1.prototype = Object.create(Base.prototype);

    function Derived2() {}
    Derived1.prototype = Object.create(Base.prototype);

If a property write for `d.x = val` sees only instances of both `Derived1` and
`Derived2` for `d`, it can optimize the write to `x` to a call to the
setter function defined on `Base`.

This optimization applies to shared setters on both pure JS objects as well
as DOM objects.

### SetProp_TypedObject

Optimizes accesses to properties on TypedObjects.

### SetProp_DefiniteSlot

Optimizes a write to a well-known regular property on an object.  For this
optimization to succeed, the property needs to be well-defined on the object.
For objects constructed by constructor functions, this means that the property
needs to be defined in the constructor, before any complex logic occurs within
the constructor.

This is the best case for a regular "field" type property that is not
turned into a constant.  It compiles down to a single CPU-level load
instruction.

    function SomeConstructor() {
        this.x = 10;    // x is a definite slot property
        this.y = 10;    // y is a definite slot property
        someComplicatedFunctionCall();
        this.z = 20;    // z is not a definite slot property.
    }

In the above example, the properties `x` and `y` can be determined to always
exist on any instance of `SomeConstructor` at definite locations, allowing
the engine to deterministically infer the position of `x` without a shape
guard.

This optimization can fail for a number of reasons.  If the types observed
at the property access are polymorphic (more than one type), this optimization
cannot succeed.  Furthermore, even if the object type is monomorphic, the
optimization will fail if the property being written is not a definite
slot as described above.

### SetProp_Unboxed

Similar to `SetProp_DefiniteSlot`.  Unboxed property writes are possible on
properties which satisfy all the characteristics of a definite slot, and
additionally have been observed to only store values of one kind of value.

Consider the following constructor:

    function Point(x, y) {
        this.x = x;
        this.y = y;
    }

If only integers are ever stored in the `x` and `y` properties,
then the instances of `Point` will be represented in an "unboxed" mode -
with the property values stored as raw 4-byte values within the object.

Objects which have the unboxed optimization are more compact.

### SetProp_InlineAccess

Optimizes a polymorphic property write where there are only a few different
types of objects seen, and the property on all of the different types is
determinable through a shape-check.

If a property write is monomorphic and the property's location is determinable
from the object's shape, but the property is not definite (see:
GetProp_DefiniteProperty), then this optimization may be used.

Alternatively, if the property write is polymorphic, but only has a few
different shapes observed at the access site, this optimization may be used.

This optimization compiles down to one-or more shape-guarded direct stores
to the object.  The following pseudocode describes the kind of machine
code generated by this optimization:

    if obj.shape == Shape1 then
      obj.slots[0] = val
    elif obj.shape == Shape2 then
      obj.slots[5] = val
    elif obj.shape == Shape3 then
      obj.slots[2] = val
    ...
    end

### SetProp_InlineCache

This is the worst-case scenario for a property access optimization.  This
strategy is used when all the others fail.  The engine simply inserts
an inline cache at the property write site.

Inline caches start off as a jump to a separate piece of code called
a "fallback".  The fallback calls into the interpreter VM (which is
very slow) to perform the operation, and then decides if the operation
can be optimized in that particular case.  If so, it generates a new
"stub" (or freestanding piece of jitcode) and changes the inline cache
to jump to the stub.  The stub attempts to optimize further occurrences
of that same kind of operation.

Inline caches are an order of magnitude slower than the other optimization
strategies, and are an indication that the type inference engine has
failed to collect enough information to guide the optimization process.

## <a name="getelem"></a>GetElement

### GetElem_TypedObject

Attempts to optimized element accesses on array Typed Objects.

### GetElem_Dense

Attempts to optimize element accesses on densely packed array objects.  Dense
arrays are arrays which do not have any 'holes'.  This means that the array
has valid values for all indexes from `0` to `length-1`.

### GetElem_TypedStatic

Attempts to optimize element accesses on a typed array that can be determined
to always refer to the same array object.  If this optimization succeeds, the
'array' object is treated as a constant, and is not looked up or retrieved from
a variable.

### GetElem_TypedArray

Attempts to optimize element accesses on a typed array.

### GetElem_String

Attempts to optimize element accesses on a string.

### GetElem_Arguments

Attempts to optimize element accesses on the `arguments` special object
available in functions.  This optimization only works if the arguments
object is used in well-understood ways within the function. The function
containing the arguments.length is allowed to use the arguments object in
the following ways without disabling this optimization:

- Access `arguments.length`
- Access `arguments.callee`
- Access individual args using `arguments[i]`
- Save `arguments` into variables, as long as those variables cannot be
  accessed by any nested function, and as long as there exists no `eval`
  anywhere within the function or nested function definitions.
- Call a function using `f.apply(obj, arguments)`

If the function contains any use of the arguments object that falls out of the
cases defined above, this optimization will be suppressed. In particular,
`arguments` cannot be returned from the function, or passed as an argument into
calls (except for the `apply` case above).

### GetElem_ArgumentsInlined

Similar to GetEelem_Arguments, but optimizes cases where the access on
`arguments` is happening within an inlined function.  In these cases, an
access of the form `arguments[i]` can be directly translated into a
direct reference to the corresponding argument value in the inlined call.

Consider the following:
    function foo(arg) {
      return bar(arg, 3);
    }
    function bar() {
      return arguments[0] + arguments[1];
    }

In the above case, if `foo` is compiled with Ion, and the call to `bar`
is inlined, then this optimization can transform the entire procedure to
the following pseudo-code:

    compiled foo(arg):
        // inlined call to bar(arg, 3) {
        return arg + 3;
        // }

### GetElem_InlineCache

This is the worst-case scenario for a element access optimization.  This
strategy is used when all the others fail.  The engine simply inserts
an inline cache at the property write site.

Inline caches start off as a jump to a separate piece of code called
a "fallback".  The fallback calls into the interpreter VM (which is
very slow) to perform the operation, and then decides if the operation
can be optimized in that particular case.  If so, it generates a new
"stub" (or freestanding piece of jitcode) and changes the inline cache
to jump to the stub.  The stub attempts to optimize further occurrences
of that same kind of operation.

Inline caches are an order of magnitude slower than the other optimization
strategies, and are an indication that the type inference engine has
failed to collect enough information to guide the optimization process.

## <a name="setelem"></a>SetElement

### SetElem_TypedObject

Attempts to optimized element writes on array Typed Objects.

### SetElem_TypedStatic

Attempts to optimize element writes on a typed array that can be determined
to always refer to the same array object.  If this optimization succeeds, the
'array' object is treated as a constant, and is not looked up or retrieved from
a variable.

### SetElem_TypedArray

Attempts to optimize element writes on a typed array.

### SetElem_Dense

Attempts to optimize element writes on densely packed array objects.  Dense
arrays are arrays which do not have any 'holes'.  This means that the array
has valid values for all indexes from `0` to `length-1`.

### SetElem_Arguments

Attempts to optimize element writes to the `arguments` special object
available in functions.  This optimization only works if the arguments
object is used in well-understood ways within the function. The function
containing the arguments.length is allowed to use the arguments object in
the following ways without disabling this optimization:

- Access `arguments.length`
- Access `arguments.callee`
- Access individual args using `arguments[i]`
- Save `arguments` into variables, as long as those variables cannot be
  accessed by any nested function, and as long as there exists no `eval`
  anywhere within the function or nested function definitions.
- Call a function using `f.apply(obj, arguments)`

If the function contains any use of the arguments object that falls out of the
cases defined above, this optimization will be suppressed. In particular,
`arguments` cannot be returned from the function, or passed as an argument into
calls (except for the `apply` case above).

### SetElem_InlineCache

This is the worst-case scenario for a element write optimization.  This
strategy is used when all the others fail.  The engine simply inserts
an inline cache at the property write site.

Inline caches start off as a jump to a separate piece of code called
a "fallback".  The fallback calls into the interpreter VM (which is
very slow) to perform the operation, and then decides if the operation
can be optimized in that particular case.  If so, it generates a new
"stub" (or freestanding piece of jitcode) and changes the inline cache
to jump to the stub.  The stub attempts to optimize further occurrences
of that same kind of operation.

Inline caches are an order of magnitude slower than the other optimization
strategies, and are an indication that the type inference engine has
failed to collect enough information to guide the optimization process.

## <a name="call"></a>Call

### Call_Inline

A function call `f(x)` usually pushes a frame onto the call stack. Inlining a
call site conceptually copies the body of the callee function and pastes it
in place of the call site and avoids pushing a new execution frame. Usually,
hot functions do well to be inlined. This is one of the most important
optimizations the JIT performs.

Ion inlines both interpreted (i.e., written in JavaScript) functions and
native (i.e., built-ins such as `Math.sin` implemented in C++).

A successfully inlined call site has the outcome Inlined.

Failure to inline comes in two flavors: unable (e.g., unable to determine
exact callee) and unwilling (e.g., heuristics concluded that the time-space
tradeoff will not pay off).
