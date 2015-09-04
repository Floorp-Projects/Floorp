# JIT Optimization Outcomes

SpiderMonkey's optimizing JIT, IonMonkey, uses a number of different
optimization strategies to speed up various operations. The most commonplace
operations that are relevant for fast program execution are property accesses
and function calls.

This page documents the meaning of different optimization outcomes.

## General Outcomes

General outcomes shared between various optimization strategies.

### GenericFailure

The optimization attempt failed, and the reason was not recorded.

### GenericSuccess

Optimization succeeded.

### Disabled

The optimization has been explicitly disallowed.

### NoTypeInfo

Optimization failed because there was no type information associated with
object containing the property. This failure mode is unlikely, and occurs
if the target object is obtained in some roundabout way.

### NoAnalysisInfo

TODO

### NoShapeInfo

The baseline compiler recorded no usable shape information for this operation.

### UnknownObject

The type of the object is not known.  This can happen if the operation sees many different types of objects, and so the type of the input to the operation cannot be resolved to a single type.

### UnknownProperties

Optimization failed because the object containing the property was marked
as having unknown properties. This can happen if too many properties are
defined on the object, or if `delete` is used to remove one of the object's
properties.

### Singleton

One of the types present in the typeset was a singleton type, preventing the optimization from being enabled.

### NotSingleton

Optimization failed because the object containing the property did not
have a 'singleton' type. Singleton types are assigned to objects that are
"one of a kind", such as global objects, literal objects declared in the
global scope, and top-level function objects.

### NotFixedSlot

The property being accessed is not stored at a known location in the object.  This can occur if one of the expected types of objects to be used in this operation has unknown properties, or if different instances of the object store the property at different locations (for example, some instances have the property assigned in a different order than others).

### InconsistentFixedSlot

The property being accessed is not stored at a known location in the object.  This can occur if the operation is polymorphic on different object types and one or more of the object types contain the property at a different slot than the others.

### NotObject

Optimization failed because the stored in the property could potentially
be a non-object value.  Since only objects can be uniquely typed, the
optimization strategy fails in this case.

### NotStruct

The object holding the property is not a typed struct object.

### NotUnboxed

The object whose property is being accessed is not formatted as an
unboxed object.

### UnboxedConvertedToNative

The object whose property is being accessed was previously unboxed,
but was deoptimized and converted to a native object.

### StructNoField

The unboxed property being accessed does not correspond to a field on typed
object.

### InconsistentFieldType

The type of an unboxed field is not consistent across all the different types of objects it could be accessed from.

### InconsistentFieldOffset

The offset of an unboxed field is not consistent across all the different types of objects it could be accessed from.

### NeedsTypeBarrier

Optimization failed because somehow the property was accessed in a way
that returned a different type than the expected constant. This is an
unlikely failure mode, and should not occur.

### InDictionaryMode

The object whose property is being accessed is in dictionary mode.  Objects which are used in ways that suggest they are hashtables, are turned into dictionary objects and their types marked as such.

### NoProtoFound

A prototype object was not found for all the object used by this operation.

### MultiProtoPaths

Objects used in this operation had differing prototypes.

### NonWritableProperty

The property being assigned to is not writable for some types of objects which are used in this operation.

### ProtoIndexedProps

The object being accessed has indexed properties that are exotic (for example, defined as a property on a prototype object and left as a hole in the underlying object).

### ArrayBadFlags

The array being accessed may have flags that the optimization strategy cannot handle.  For example, if the array has sparse indexes, or has indexes that overflow the array's length, the optimization strategy may fail.

### ArrayDoubleConversion

The type-system indicates that some arrays at this site should be converted to packed arrays of doubles, while others should not.  The optimization strategy fails for this condition.

### ArrayRange

Could not accurately calculate the range attributes of an inline array creation.

### ArraySeenNegativeIndex

Arrays at this element access location have seen negative indexes.

### TypedObjectNeutered

The typed object being accessed at this location may have been neutered (a neutered typed object is one where the underlying byte buffer has been removed or transferred to a worker).

### TypedObjectArrayRange

Failed to do range check of element access on a typed object.

### AccessNotDense

### AccessNotSimdObject

The observed type of the target of the property access doesn't guarantee
that it is a SIMD object.

### AccessNotTypedObject

The observed type of the target of the property access doesn't guarantee
that it is a TypedObject.

### AccessNotTypedArray
### AccessNotString
### StaticTypedArrayUint32
### StaticTypedArrayCantComputeMask
### OutOfBounds
### GetElemStringNotCached
### NonNativeReceiver
### IndexType
### SetElemNonDenseNonTANotCached

### NoSimdJitSupport

Optimization failed because SIMD JIT support was not enabled.

### SimdTypeNotOptimized

The type observed as being retrieved from this property access did not
match an optimizable type.

### UnknownSimdProperty

The property being accessed on the object is not one of the optimizable
property names.

### HasCommonInliningPath

### Inlined
### DOM
### Monomorphic
### Polymorphic


## Inline Cache Outcomes

Outcomes describing inline cache stubs that were generated.

### ICOptStub_GenericSuccess
### ICGetPropStub_ReadSlot
### ICGetPropStub_CallGetter
### ICGetPropStub_ArrayLength
### ICGetPropStub_UnboxedRead
### ICGetPropStub_UnboxedReadExpando
### ICGetPropStub_UnboxedArrayLength
### ICGetPropStub_TypedArrayLength
### ICGetPropStub_DOMProxyShadowed
### ICGetPropStub_DOMProxyUnshadowed
### ICGetPropStub_GenericProxy
### ICGetPropStub_ArgumentsLength

### ICSetPropStub_Slot
### ICSetPropStub_GenericProxy
### ICSetPropStub_DOMProxyShadowed
### ICSetPropStub_DOMProxyUnshadowed
### ICSetPropStub_CallSetter
### ICSetPropStub_AddSlot
### ICSetPropStub_SetUnboxed

### ICGetElemStub_ReadSlot
### ICGetElemStub_CallGetter
### ICGetElemStub_ReadUnboxed
### ICGetElemStub_Dense
### ICGetElemStub_DenseHole
### ICGetElemStub_TypedArray
### ICGetElemStub_ArgsElement
### ICGetElemStub_ArgsElementStrict

### ICSetElemStub_Dense
### ICSetElemStub_TypedArray

### ICNameStub_ReadSlot
### ICNameStub_CallGetter

## Call Inlining Outcomes

Optimization outcomes of attempts to inline function calls.

### CantInlineGeneric
### CantInlineClassConstructor
### CantInlineExceededDepth
### CantInlineExceededTotalBytecodeLength
### CantInlineBigCaller
### CantInlineBigCallee
### CantInlineBigCalleeInlinedBytecodeLength
### CantInlineNotHot
### CantInlineNotInDispatch
### CantInlineNativeBadType

### CantInlineNoTarget

Unable to inline function call. The callee (target) function, f in f(x),
is not known at JIT time.

### CantInlineNotInterpreted

Unable to inline function call. The callee function is not an interpreted
function. For example, it could be a native function for which Ion has no
built-in specialization.

### CantInlineNoBaseline

Unable to inline function call. The interpreted callee function could not
be compiled by the Baseline compiler.

### CantInlineLazy

Unable to inline function call. The interpreted callee function has a
lazy, compiled on-demand script instead of an already compiled script.

### CantInlineNotConstructor

Unable to inline function call. Rare. The interpreted callee function is
invoked with new but cannot be called as a constructor. Property
accessors, Function.prototype, and arrow (=>) functions cannot be called
as constructors.

### CantInlineDisableIon

Unable to inline function call. The interpreted callee function has been
explicitly blacklisted against Ion compilation.

### CantInlineTooManyArgs

Unable to inline function call. The interpreted callee function either has
too many parameters or is called with too many arguments. These thresholds
are subject to change.

### CantInlineRecursive

Unable to inline function call. The interpreted callee function recurs for
more than one level.  The first level of recursion is inlineable.

### CantInlineHeavyweight

Unable to inline function call. The interpreted callee function contains
variables bindings that are closed over. For example,
`function f() { var x; return function () { x++; } }`
closes over x in f.

### CantInlineNeedsArgsObj

Unable to inline function call. The interpreted callee function requires
an arguments object to be created.

### CantInlineDebuggee

Unable to inline function call. The interpreted callee function is being
debugged by the Debugger API.

### CantInlineUnknownProps

Unable to inline function call. The engine knows nothing definite about
the type of the callee function object.

### CantInlineUnreachable

Unable to inline function call. The call site has not been observed to
have ever been executed.  It lacks observed type information for its arguments,
its return value, or both.

### CantInlineBound

Unable to inline function call. The interpreted callee is a bound function
generated from `Function.prototype.bind` that failed some sub-checks.
(expand)

### CantInlineNativeNoSpecialization

Unable to inline function call. Ion does not have built-in specialization for
the native (implemented in C++) callee function.

### CantInlineNativeBadForm

Unable to inline function call. The native callee function was called with an
unsupported number of arguments, or calling non-constructing functions with new.

### CantInlineBadType

Unable to inline function call. The native callee function was called with
arguments with types that the built-in specialization does not support. For
example, calling Math functions on objects.

### CantInlineNativeNoTemplateObj

Unable to inline function call. Cannot inline a native constructor (e.g., new
Array) because no template object was cached by the Baseline compiler. (expand)
