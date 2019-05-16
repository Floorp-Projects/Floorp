/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_VMFunctions_h
#define jit_VMFunctions_h

#include "mozilla/Attributes.h"
#include "mozilla/HashFunctions.h"

#include "jspubtd.h"

#include "jit/CompileInfo.h"
#include "jit/JitFrames.h"
#include "vm/Interpreter.h"

namespace js {

class ArgumentsObject;
class NamedLambdaObject;
class WithScope;
class InlineTypedObject;
class AbstractGeneratorObject;
class AsyncFunctionGeneratorObject;
class RegExpObject;
class TypedArrayObject;

namespace gc {

struct Cell;

}

namespace jit {

enum DataType : uint8_t {
  Type_Void,
  Type_Bool,
  Type_Int32,
  Type_Double,
  Type_Pointer,
  Type_Object,
  Type_Value,
  Type_Handle
};

struct PopValues {
  uint8_t numValues;

  explicit constexpr PopValues(uint8_t numValues) : numValues(numValues) {}
};

enum MaybeTailCall : bool { TailCall, NonTailCall };

// [SMDOC] JIT-to-C++ Function Calls. (callVM)
//
// Sometimes it is easier to reuse C++ code by calling VM's functions. Calling a
// function from the VM can be achieved with the use of callWithABI but this is
// discouraged when the called functions might trigger exceptions and/or
// garbage collections which are expecting to walk the stack. VMFunctions and
// callVM are interfaces provided to handle the exception handling and register
// the stack end (JITActivation) such that walking the stack is made possible.
//
// VMFunctionData is a structure which contains the necessary information needed
// for generating a trampoline function to make a call (with generateVMWrapper)
// and to root the arguments of the function (in TraceJitExitFrame).
// VMFunctionData is created with the VMFunctionDataHelper template, which
// infers the VMFunctionData fields from the function signature. The rooting and
// trampoline code is therefore determined by the arguments of a function and
// their locations in the signature of a function.
//
// VM functions all expect a JSContext* as first argument. This argument is
// implicitly provided by the trampoline code (in generateVMWrapper) and used
// for creating new objects or reporting errors. If your function does not make
// use of a JSContext* argument, then you might probably use a callWithABI
// call.
//
// Functions described using the VMFunction system must conform to a simple
// protocol: the return type must have a special "failure" value (for example,
// false for bool, or nullptr for Objects). If the function is designed to
// return a value that does not meet this requirement - such as
// object-or-nullptr, or an integer, an optional, final outParam can be
// specified. In this case, the return type must be boolean to indicate
// failure.
//
// JIT Code usage:
//
// Different JIT compilers in SpiderMonkey have their own implementations of
// callVM to call VM functions. However, the general shape of them is that
// arguments (excluding the JSContext or trailing out-param) are pushed on to
// the stack from right to left (rightmost argument is pushed first).
//
// Regardless of return value protocol being used (final outParam, or return
// value) the generated trampolines ensure the return value ends up in
// JSReturnOperand, ReturnReg or ReturnDoubleReg.
//
// Example:
//
// The details will differ slightly between the different compilers in
// SpiderMonkey, but the general shape of our usage looks like this:
//
// Suppose we have a function Foo:
//
//      bool Foo(JSContext* cx, HandleObject x, HandleId y,
//               MutableHandleValue z);
//
// This function returns true on success, and z is the outparam return value.
//
// A VM function wrapper for this can be created by adding an entry to
// VM_FUNCTION_LIST in VMFunctionList-inl.h:
//
//    _(Foo, js::Foo)
//
// In the compiler code the call would then be issued like this:
//
//      masm.Push(id);
//      masm.Push(obj);
//
//      using Fn = bool (*)(JSContext*, HandleObject, HandleId,
//                          MutableHandleValue);
//      if (!callVM<Fn, js::Foo>()) {
//          return false;
//      }
//
// After this, the result value is in the return value register.

// Data for a VM function. All VMFunctionDatas are stored in a constexpr array.
struct VMFunctionData {
#if defined(JS_JITSPEW) || defined(JS_TRACE_LOGGING)
  // Informative name of the wrapped function. The name should not be present
  // in release builds in order to save memory.
  const char* name_;
#endif

  // Note: a maximum of seven root types is supported.
  enum RootType : uint8_t {
    RootNone = 0,
    RootObject,
    RootString,
    RootId,
    RootFunction,
    RootValue,
    RootCell
  };

  // Contains an combination of enumerated types used by the gc for marking
  // arguments of the VM wrapper.
  uint64_t argumentRootTypes;

  enum ArgProperties {
    WordByValue = 0,
    DoubleByValue = 1,
    WordByRef = 2,
    DoubleByRef = 3,
    // BitMask version.
    Word = 0,
    Double = 1,
    ByRef = 2
  };

  // Contains properties about the first 16 arguments.
  uint32_t argumentProperties;

  // Which arguments should be passed in float register on platforms that
  // have them.
  uint32_t argumentPassedInFloatRegs;

  // Number of arguments expected, excluding JSContext * as an implicit
  // first argument and an outparam as a possible implicit final argument.
  uint8_t explicitArgs;

  // The root type of the out param if outParam == Type_Handle.
  RootType outParamRootType;

  // The outparam may be any Type_*, and must be the final argument to the
  // function, if not Void. outParam != Void implies that the return type
  // has a boolean failure mode.
  DataType outParam;

  // Type returned by the C function and used by the VMFunction wrapper to
  // check for failures of the C function.  Valid failure/return types are
  // boolean and object pointers which are asserted inside the VMFunction
  // constructor. If the C function use an outparam (!= Type_Void), then
  // the only valid failure/return type is boolean -- object pointers are
  // pointless because the wrapper will only use it to compare it against
  // nullptr before discarding its value.
  DataType returnType;

  // Number of Values the VM wrapper should pop from the stack when it returns.
  // Used by baseline IC stubs so that they can use tail calls to call the VM
  // wrapper.
  uint8_t extraValuesToPop;

  // On some architectures, called functions need to explicitly push their
  // return address, for a tail call, there is nothing to push, so tail-callness
  // needs to be known at compile time.
  MaybeTailCall expectTailCall;

  uint32_t argc() const {
    // JSContext * + args + (OutParam? *)
    return 1 + explicitArgc() + ((outParam == Type_Void) ? 0 : 1);
  }

  DataType failType() const { return returnType; }

  // Whether this function returns anything more than a boolean flag for
  // failures.
  bool returnsData() const {
    return returnType == Type_Object || outParam != Type_Void;
  }

  ArgProperties argProperties(uint32_t explicitArg) const {
    return ArgProperties((argumentProperties >> (2 * explicitArg)) & 3);
  }

  RootType argRootType(uint32_t explicitArg) const {
    return RootType((argumentRootTypes >> (3 * explicitArg)) & 7);
  }

  bool argPassedInFloatReg(uint32_t explicitArg) const {
    return ((argumentPassedInFloatRegs >> explicitArg) & 1) == 1;
  }

#if defined(JS_JITSPEW) || defined(JS_TRACE_LOGGING)
  const char* name() const { return name_; }
#endif

  // Return the stack size consumed by explicit arguments.
  size_t explicitStackSlots() const {
    size_t stackSlots = explicitArgs;

    // Fetch all double-word flags of explicit arguments.
    uint32_t n = ((1 << (explicitArgs * 2)) - 1)  // = Explicit argument mask.
                 & 0x55555555                     // = Mask double-size args.
                 & argumentProperties;

    // Add the number of double-word flags. (expect a few loop
    // iteration)
    while (n) {
      stackSlots++;
      n &= n - 1;
    }
    return stackSlots;
  }

  // Double-size argument which are passed by value are taking the space
  // of 2 C arguments.  This function is used to compute the number of
  // argument expected by the C function.  This is not the same as
  // explicitStackSlots because reference to stack slots may take one less
  // register in the total count.
  size_t explicitArgc() const {
    size_t stackSlots = explicitArgs;

    // Fetch all explicit arguments.
    uint32_t n = ((1 << (explicitArgs * 2)) - 1)  // = Explicit argument mask.
                 & argumentProperties;

    // Filter double-size arguments (0x5 = 0b0101) and remove (& ~)
    // arguments passed by reference (0b1010 >> 1 == 0b0101).
    n = (n & 0x55555555) & ~(n >> 1);

    // Add the number of double-word transfered by value. (expect a few
    // loop iteration)
    while (n) {
      stackSlots++;
      n &= n - 1;
    }
    return stackSlots;
  }

  size_t doubleByRefArgs() const {
    size_t count = 0;

    // Fetch all explicit arguments.
    uint32_t n = ((1 << (explicitArgs * 2)) - 1)  // = Explicit argument mask.
                 & argumentProperties;

    // Filter double-size arguments (0x5 = 0b0101) and take (&) only
    // arguments passed by reference (0b1010 >> 1 == 0b0101).
    n = (n & 0x55555555) & (n >> 1);

    // Add the number of double-word transfered by refference. (expect a
    // few loop iterations)
    while (n) {
      count++;
      n &= n - 1;
    }
    return count;
  }

  constexpr VMFunctionData(const char* name, uint32_t explicitArgs,
                           uint32_t argumentProperties,
                           uint32_t argumentPassedInFloatRegs,
                           uint64_t argRootTypes, DataType outParam,
                           RootType outParamRootType, DataType returnType,
                           uint8_t extraValuesToPop = 0,
                           MaybeTailCall expectTailCall = NonTailCall)
      :
#if defined(JS_JITSPEW) || defined(JS_TRACE_LOGGING)
        name_(name),
#endif
        argumentRootTypes(argRootTypes),
        argumentProperties(argumentProperties),
        argumentPassedInFloatRegs(argumentPassedInFloatRegs),
        explicitArgs(explicitArgs),
        outParamRootType(outParamRootType),
        outParam(outParam),
        returnType(returnType),
        extraValuesToPop(extraValuesToPop),
        expectTailCall(expectTailCall) {
    // Check for valid failure/return type.
    MOZ_ASSERT_IF(outParam != Type_Void,
                  returnType == Type_Void || returnType == Type_Bool);
    MOZ_ASSERT(returnType == Type_Void || returnType == Type_Bool ||
               returnType == Type_Object);
  }

  constexpr VMFunctionData(const VMFunctionData& o) = default;
};

template <class>
struct TypeToDataType { /* Unexpected return type for a VMFunction. */
};
template <>
struct TypeToDataType<void> {
  static const DataType result = Type_Void;
};
template <>
struct TypeToDataType<bool> {
  static const DataType result = Type_Bool;
};
template <>
struct TypeToDataType<JSObject*> {
  static const DataType result = Type_Object;
};
template <>
struct TypeToDataType<JSFunction*> {
  static const DataType result = Type_Object;
};
template <>
struct TypeToDataType<NativeObject*> {
  static const DataType result = Type_Object;
};
template <>
struct TypeToDataType<PlainObject*> {
  static const DataType result = Type_Object;
};
template <>
struct TypeToDataType<InlineTypedObject*> {
  static const DataType result = Type_Object;
};
template <>
struct TypeToDataType<NamedLambdaObject*> {
  static const DataType result = Type_Object;
};
template <>
struct TypeToDataType<LexicalEnvironmentObject*> {
  static const DataType result = Type_Object;
};
template <>
struct TypeToDataType<ArgumentsObject*> {
  static const DataType result = Type_Object;
};
template <>
struct TypeToDataType<ArrayObject*> {
  static const DataType result = Type_Object;
};
template <>
struct TypeToDataType<TypedArrayObject*> {
  static const DataType result = Type_Object;
};
template <>
struct TypeToDataType<ArrayIteratorObject*> {
  static const DataType result = Type_Object;
};
template <>
struct TypeToDataType<StringIteratorObject*> {
  static const DataType result = Type_Object;
};
template <>
struct TypeToDataType<RegExpStringIteratorObject*> {
  static const DataType result = Type_Object;
};
template <>
struct TypeToDataType<JSString*> {
  static const DataType result = Type_Object;
};
template <>
struct TypeToDataType<JSFlatString*> {
  static const DataType result = Type_Object;
};
template <>
struct TypeToDataType<HandleObject> {
  static const DataType result = Type_Handle;
};
template <>
struct TypeToDataType<HandleString> {
  static const DataType result = Type_Handle;
};
template <>
struct TypeToDataType<HandlePropertyName> {
  static const DataType result = Type_Handle;
};
template <>
struct TypeToDataType<HandleFunction> {
  static const DataType result = Type_Handle;
};
template <>
struct TypeToDataType<Handle<NativeObject*> > {
  static const DataType result = Type_Handle;
};
template <>
struct TypeToDataType<Handle<InlineTypedObject*> > {
  static const DataType result = Type_Handle;
};
template <>
struct TypeToDataType<Handle<ArrayObject*> > {
  static const DataType result = Type_Handle;
};
template <>
struct TypeToDataType<Handle<AbstractGeneratorObject*> > {
  static const DataType result = Type_Handle;
};
template <>
struct TypeToDataType<Handle<AsyncFunctionGeneratorObject*> > {
  static const DataType result = Type_Handle;
};
template <>
struct TypeToDataType<Handle<PlainObject*> > {
  static const DataType result = Type_Handle;
};
template <>
struct TypeToDataType<Handle<WithScope*> > {
  static const DataType result = Type_Handle;
};
template <>
struct TypeToDataType<Handle<LexicalScope*> > {
  static const DataType result = Type_Handle;
};
template <>
struct TypeToDataType<Handle<Scope*> > {
  static const DataType result = Type_Handle;
};
template <>
struct TypeToDataType<HandleScript> {
  static const DataType result = Type_Handle;
};
template <>
struct TypeToDataType<HandleValue> {
  static const DataType result = Type_Handle;
};
template <>
struct TypeToDataType<MutableHandleValue> {
  static const DataType result = Type_Handle;
};
template <>
struct TypeToDataType<HandleId> {
  static const DataType result = Type_Handle;
};

// Convert argument types to properties of the argument known by the jit.
template <class T>
struct TypeToArgProperties {
  static const uint32_t result =
      (sizeof(T) <= sizeof(void*) ? VMFunctionData::Word
                                  : VMFunctionData::Double);
};
template <>
struct TypeToArgProperties<const Value&> {
  static const uint32_t result =
      TypeToArgProperties<Value>::result | VMFunctionData::ByRef;
};
template <>
struct TypeToArgProperties<HandleObject> {
  static const uint32_t result =
      TypeToArgProperties<JSObject*>::result | VMFunctionData::ByRef;
};
template <>
struct TypeToArgProperties<HandleString> {
  static const uint32_t result =
      TypeToArgProperties<JSString*>::result | VMFunctionData::ByRef;
};
template <>
struct TypeToArgProperties<HandlePropertyName> {
  static const uint32_t result =
      TypeToArgProperties<PropertyName*>::result | VMFunctionData::ByRef;
};
template <>
struct TypeToArgProperties<HandleFunction> {
  static const uint32_t result =
      TypeToArgProperties<JSFunction*>::result | VMFunctionData::ByRef;
};
template <>
struct TypeToArgProperties<Handle<NativeObject*> > {
  static const uint32_t result =
      TypeToArgProperties<NativeObject*>::result | VMFunctionData::ByRef;
};
template <>
struct TypeToArgProperties<Handle<InlineTypedObject*> > {
  static const uint32_t result =
      TypeToArgProperties<InlineTypedObject*>::result | VMFunctionData::ByRef;
};
template <>
struct TypeToArgProperties<Handle<ArrayObject*> > {
  static const uint32_t result =
      TypeToArgProperties<ArrayObject*>::result | VMFunctionData::ByRef;
};
template <>
struct TypeToArgProperties<Handle<AbstractGeneratorObject*> > {
  static const uint32_t result =
      TypeToArgProperties<AbstractGeneratorObject*>::result |
      VMFunctionData::ByRef;
};
template <>
struct TypeToArgProperties<Handle<AsyncFunctionGeneratorObject*> > {
  static const uint32_t result =
      TypeToArgProperties<AsyncFunctionGeneratorObject*>::result |
      VMFunctionData::ByRef;
};
template <>
struct TypeToArgProperties<Handle<PlainObject*> > {
  static const uint32_t result =
      TypeToArgProperties<PlainObject*>::result | VMFunctionData::ByRef;
};
template <>
struct TypeToArgProperties<Handle<RegExpObject*> > {
  static const uint32_t result =
      TypeToArgProperties<RegExpObject*>::result | VMFunctionData::ByRef;
};
template <>
struct TypeToArgProperties<Handle<WithScope*> > {
  static const uint32_t result =
      TypeToArgProperties<WithScope*>::result | VMFunctionData::ByRef;
};
template <>
struct TypeToArgProperties<Handle<LexicalScope*> > {
  static const uint32_t result =
      TypeToArgProperties<LexicalScope*>::result | VMFunctionData::ByRef;
};
template <>
struct TypeToArgProperties<Handle<Scope*> > {
  static const uint32_t result =
      TypeToArgProperties<Scope*>::result | VMFunctionData::ByRef;
};
template <>
struct TypeToArgProperties<HandleScript> {
  static const uint32_t result =
      TypeToArgProperties<JSScript*>::result | VMFunctionData::ByRef;
};
template <>
struct TypeToArgProperties<HandleValue> {
  static const uint32_t result =
      TypeToArgProperties<Value>::result | VMFunctionData::ByRef;
};
template <>
struct TypeToArgProperties<MutableHandleValue> {
  static const uint32_t result =
      TypeToArgProperties<Value>::result | VMFunctionData::ByRef;
};
template <>
struct TypeToArgProperties<HandleId> {
  static const uint32_t result =
      TypeToArgProperties<jsid>::result | VMFunctionData::ByRef;
};
template <>
struct TypeToArgProperties<HandleShape> {
  static const uint32_t result =
      TypeToArgProperties<Shape*>::result | VMFunctionData::ByRef;
};
template <>
struct TypeToArgProperties<HandleObjectGroup> {
  static const uint32_t result =
      TypeToArgProperties<ObjectGroup*>::result | VMFunctionData::ByRef;
};

// Convert argument type to whether or not it should be passed in a float
// register on platforms that have them, like x64.
template <class T>
struct TypeToPassInFloatReg {
  static const uint32_t result = 0;
};
template <>
struct TypeToPassInFloatReg<double> {
  static const uint32_t result = 1;
};

// Convert argument types to root types used by the gc, see MarkJitExitFrame.
template <class T>
struct TypeToRootType {
  static const uint32_t result = VMFunctionData::RootNone;
};
template <>
struct TypeToRootType<HandleObject> {
  static const uint32_t result = VMFunctionData::RootObject;
};
template <>
struct TypeToRootType<HandleString> {
  static const uint32_t result = VMFunctionData::RootString;
};
template <>
struct TypeToRootType<HandlePropertyName> {
  static const uint32_t result = VMFunctionData::RootString;
};
template <>
struct TypeToRootType<HandleFunction> {
  static const uint32_t result = VMFunctionData::RootFunction;
};
template <>
struct TypeToRootType<HandleValue> {
  static const uint32_t result = VMFunctionData::RootValue;
};
template <>
struct TypeToRootType<MutableHandleValue> {
  static const uint32_t result = VMFunctionData::RootValue;
};
template <>
struct TypeToRootType<HandleId> {
  static const uint32_t result = VMFunctionData::RootId;
};
template <>
struct TypeToRootType<HandleShape> {
  static const uint32_t result = VMFunctionData::RootCell;
};
template <>
struct TypeToRootType<HandleObjectGroup> {
  static const uint32_t result = VMFunctionData::RootCell;
};
template <>
struct TypeToRootType<HandleScript> {
  static const uint32_t result = VMFunctionData::RootCell;
};
template <>
struct TypeToRootType<Handle<NativeObject*> > {
  static const uint32_t result = VMFunctionData::RootObject;
};
template <>
struct TypeToRootType<Handle<InlineTypedObject*> > {
  static const uint32_t result = VMFunctionData::RootObject;
};
template <>
struct TypeToRootType<Handle<ArrayObject*> > {
  static const uint32_t result = VMFunctionData::RootObject;
};
template <>
struct TypeToRootType<Handle<AbstractGeneratorObject*> > {
  static const uint32_t result = VMFunctionData::RootObject;
};
template <>
struct TypeToRootType<Handle<AsyncFunctionGeneratorObject*> > {
  static const uint32_t result = VMFunctionData::RootObject;
};
template <>
struct TypeToRootType<Handle<PlainObject*> > {
  static const uint32_t result = VMFunctionData::RootObject;
};
template <>
struct TypeToRootType<Handle<RegExpObject*> > {
  static const uint32_t result = VMFunctionData::RootObject;
};
template <>
struct TypeToRootType<Handle<LexicalScope*> > {
  static const uint32_t result = VMFunctionData::RootCell;
};
template <>
struct TypeToRootType<Handle<WithScope*> > {
  static const uint32_t result = VMFunctionData::RootCell;
};
template <>
struct TypeToRootType<Handle<Scope*> > {
  static const uint32_t result = VMFunctionData::RootCell;
};
template <class T>
struct TypeToRootType<Handle<T> > {
  // Fail for Handle types that aren't specialized above.
};

template <class>
struct OutParamToDataType {
  static const DataType result = Type_Void;
};
template <>
struct OutParamToDataType<Value*> {
  static const DataType result = Type_Value;
};
template <>
struct OutParamToDataType<int*> {
  static const DataType result = Type_Int32;
};
template <>
struct OutParamToDataType<uint32_t*> {
  static const DataType result = Type_Int32;
};
template <>
struct OutParamToDataType<uint8_t**> {
  static const DataType result = Type_Pointer;
};
template <>
struct OutParamToDataType<bool*> {
  static const DataType result = Type_Bool;
};
template <>
struct OutParamToDataType<double*> {
  static const DataType result = Type_Double;
};
template <>
struct OutParamToDataType<MutableHandleValue> {
  static const DataType result = Type_Handle;
};
template <>
struct OutParamToDataType<MutableHandleObject> {
  static const DataType result = Type_Handle;
};
template <>
struct OutParamToDataType<MutableHandleString> {
  static const DataType result = Type_Handle;
};

template <class>
struct OutParamToRootType {
  static const VMFunctionData::RootType result = VMFunctionData::RootNone;
};
template <>
struct OutParamToRootType<MutableHandleValue> {
  static const VMFunctionData::RootType result = VMFunctionData::RootValue;
};
template <>
struct OutParamToRootType<MutableHandleObject> {
  static const VMFunctionData::RootType result = VMFunctionData::RootObject;
};
template <>
struct OutParamToRootType<MutableHandleString> {
  static const VMFunctionData::RootType result = VMFunctionData::RootString;
};

// Extract the last element of a list of types.
template <typename... ArgTypes>
struct LastArg;

template <>
struct LastArg<> {
  typedef void Type;
  static constexpr size_t nbArgs = 0;
};

template <typename HeadType>
struct LastArg<HeadType> {
  typedef HeadType Type;
  static constexpr size_t nbArgs = 1;
};

template <typename HeadType, typename... TailTypes>
struct LastArg<HeadType, TailTypes...> {
  typedef typename LastArg<TailTypes...>::Type Type;
  static constexpr size_t nbArgs = LastArg<TailTypes...>::nbArgs + 1;
};

// Construct a bit mask from a list of types.  The mask is constructed as an OR
// of the mask produced for each argument. The result of each argument is
// shifted by its index, such that the result of the first argument is on the
// low bits of the mask, and the result of the last argument in part of the
// high bits of the mask.
template <template <typename> class Each, typename ResultType, size_t Shift,
          typename... Args>
struct BitMask;

template <template <typename> class Each, typename ResultType, size_t Shift>
struct BitMask<Each, ResultType, Shift> {
  static constexpr ResultType result = ResultType();
};

template <template <typename> class Each, typename ResultType, size_t Shift,
          typename HeadType, typename... TailTypes>
struct BitMask<Each, ResultType, Shift, HeadType, TailTypes...> {
  static_assert(ResultType(Each<HeadType>::result) < (1 << Shift),
                "not enough bits reserved by the shift for individual results");
  static_assert(LastArg<TailTypes...>::nbArgs <
                    (8 * sizeof(ResultType) / Shift),
                "not enough bits in the result type to store all bit masks");

  static constexpr ResultType result =
      ResultType(Each<HeadType>::result) |
      (BitMask<Each, ResultType, Shift, TailTypes...>::result << Shift);
};

class AutoDetectInvalidation {
  JSContext* cx_;
  IonScript* ionScript_;
  MutableHandleValue rval_;
  bool disabled_;

  void setReturnOverride();

 public:
  AutoDetectInvalidation(JSContext* cx, MutableHandleValue rval,
                         IonScript* ionScript)
      : cx_(cx), ionScript_(ionScript), rval_(rval), disabled_(false) {
    MOZ_ASSERT(ionScript);
  }

  AutoDetectInvalidation(JSContext* cx, MutableHandleValue rval);

  void disable() {
    MOZ_ASSERT(!disabled_);
    disabled_ = true;
  }

  bool shouldSetReturnOverride() const {
    return !disabled_ && ionScript_->invalidated();
  }

  ~AutoDetectInvalidation() {
    if (MOZ_UNLIKELY(shouldSetReturnOverride())) {
      setReturnOverride();
    }
  }
};

MOZ_MUST_USE bool InvokeFunction(JSContext* cx, HandleObject obj0,
                                 bool constructing, bool ignoresReturnValue,
                                 uint32_t argc, Value* argv,
                                 MutableHandleValue rval);
MOZ_MUST_USE bool InvokeFunctionShuffleNewTarget(
    JSContext* cx, HandleObject obj, uint32_t numActualArgs,
    uint32_t numFormalArgs, Value* argv, MutableHandleValue rval);

class InterpreterStubExitFrameLayout;
bool InvokeFromInterpreterStub(JSContext* cx,
                               InterpreterStubExitFrameLayout* frame);

bool CheckOverRecursed(JSContext* cx);
bool CheckOverRecursedBaseline(JSContext* cx, BaselineFrame* frame);

MOZ_MUST_USE bool MutatePrototype(JSContext* cx, HandlePlainObject obj,
                                  HandleValue value);
MOZ_MUST_USE bool InitProp(JSContext* cx, HandleObject obj,
                           HandlePropertyName name, HandleValue value,
                           jsbytecode* pc);

enum class EqualityKind : bool { NotEqual, Equal };

template <EqualityKind Kind>
bool LooselyEqual(JSContext* cx, MutableHandleValue lhs, MutableHandleValue rhs,
                  bool* res);

template <EqualityKind Kind>
bool StrictlyEqual(JSContext* cx, MutableHandleValue lhs,
                   MutableHandleValue rhs, bool* res);

bool LessThan(JSContext* cx, MutableHandleValue lhs, MutableHandleValue rhs,
              bool* res);
bool LessThanOrEqual(JSContext* cx, MutableHandleValue lhs,
                     MutableHandleValue rhs, bool* res);
bool GreaterThan(JSContext* cx, MutableHandleValue lhs, MutableHandleValue rhs,
                 bool* res);
bool GreaterThanOrEqual(JSContext* cx, MutableHandleValue lhs,
                        MutableHandleValue rhs, bool* res);

template <EqualityKind Kind>
bool StringsEqual(JSContext* cx, HandleString lhs, HandleString rhs, bool* res);

enum class ComparisonKind : bool { GreaterThanOrEqual, LessThan };

template <ComparisonKind Kind>
bool StringsCompare(JSContext* cx, HandleString lhs, HandleString rhs,
                    bool* res);

MOZ_MUST_USE bool ArrayPopDense(JSContext* cx, HandleObject obj,
                                MutableHandleValue rval);
MOZ_MUST_USE bool ArrayPushDense(JSContext* cx, HandleArrayObject arr,
                                 HandleValue v, uint32_t* length);
MOZ_MUST_USE bool ArrayShiftDense(JSContext* cx, HandleObject obj,
                                  MutableHandleValue rval);
JSString* ArrayJoin(JSContext* cx, HandleObject array, HandleString sep);
MOZ_MUST_USE bool SetArrayLength(JSContext* cx, HandleObject obj,
                                 HandleValue value, bool strict);

MOZ_MUST_USE bool CharCodeAt(JSContext* cx, HandleString str, int32_t index,
                             uint32_t* code);
JSFlatString* StringFromCharCode(JSContext* cx, int32_t code);
JSString* StringFromCodePoint(JSContext* cx, int32_t codePoint);

MOZ_MUST_USE bool SetProperty(JSContext* cx, HandleObject obj,
                              HandlePropertyName name, HandleValue value,
                              bool strict, jsbytecode* pc);

MOZ_MUST_USE bool InterruptCheck(JSContext* cx);

void* MallocWrapper(JS::Zone* zone, size_t nbytes);
JSObject* NewCallObject(JSContext* cx, HandleShape shape,
                        HandleObjectGroup group);
JSObject* NewStringObject(JSContext* cx, HandleString str);

bool OperatorIn(JSContext* cx, HandleValue key, HandleObject obj, bool* out);
bool OperatorInI(JSContext* cx, uint32_t index, HandleObject obj, bool* out);

MOZ_MUST_USE bool GetIntrinsicValue(JSContext* cx, HandlePropertyName name,
                                    MutableHandleValue rval);

MOZ_MUST_USE bool CreateThis(JSContext* cx, HandleObject callee,
                             HandleObject newTarget, MutableHandleValue rval);

bool GetDynamicNamePure(JSContext* cx, JSObject* scopeChain, JSString* str,
                        Value* vp);

void PostWriteBarrier(JSRuntime* rt, js::gc::Cell* cell);
void PostGlobalWriteBarrier(JSRuntime* rt, GlobalObject* obj);

enum class IndexInBounds { Yes, Maybe };

template <IndexInBounds InBounds>
void PostWriteElementBarrier(JSRuntime* rt, JSObject* obj, int32_t index);

// If |str| is an index in the range [0, INT32_MAX], return it. If the string
// is not an index in this range, return -1.
int32_t GetIndexFromString(JSString* str);

JSObject* WrapObjectPure(JSContext* cx, JSObject* obj);

MOZ_MUST_USE bool DebugPrologue(JSContext* cx, BaselineFrame* frame,
                                jsbytecode* pc, bool* mustReturn);
MOZ_MUST_USE bool DebugEpilogue(JSContext* cx, BaselineFrame* frame,
                                jsbytecode* pc, bool ok);
MOZ_MUST_USE bool DebugEpilogueOnBaselineReturn(JSContext* cx,
                                                BaselineFrame* frame,
                                                jsbytecode* pc);
void FrameIsDebuggeeCheck(BaselineFrame* frame);

JSObject* CreateGenerator(JSContext* cx, BaselineFrame* frame);

MOZ_MUST_USE bool NormalSuspend(JSContext* cx, HandleObject obj,
                                BaselineFrame* frame, jsbytecode* pc);
MOZ_MUST_USE bool FinalSuspend(JSContext* cx, HandleObject obj, jsbytecode* pc);
MOZ_MUST_USE bool InterpretResume(JSContext* cx, HandleObject obj,
                                  HandleValue val, HandlePropertyName kind,
                                  MutableHandleValue rval);
MOZ_MUST_USE bool DebugAfterYield(JSContext* cx, BaselineFrame* frame,
                                  jsbytecode* pc, bool* mustReturn);
MOZ_MUST_USE bool GeneratorThrowOrReturn(
    JSContext* cx, BaselineFrame* frame,
    Handle<AbstractGeneratorObject*> genObj, HandleValue arg,
    uint32_t resumeKindArg);

MOZ_MUST_USE bool GlobalNameConflictsCheckFromIon(JSContext* cx,
                                                  HandleScript script);
MOZ_MUST_USE bool InitFunctionEnvironmentObjects(JSContext* cx,
                                                 BaselineFrame* frame);

MOZ_MUST_USE bool NewArgumentsObject(JSContext* cx, BaselineFrame* frame,
                                     MutableHandleValue res);

JSObject* CopyLexicalEnvironmentObject(JSContext* cx, HandleObject env,
                                       bool copySlots);

JSObject* InitRestParameter(JSContext* cx, uint32_t length, Value* rest,
                            HandleObject templateObj, HandleObject res);

MOZ_MUST_USE bool HandleDebugTrap(JSContext* cx, BaselineFrame* frame,
                                  uint8_t* retAddr, bool* mustReturn);
MOZ_MUST_USE bool OnDebuggerStatement(JSContext* cx, BaselineFrame* frame,
                                      jsbytecode* pc, bool* mustReturn);
MOZ_MUST_USE bool GlobalHasLiveOnDebuggerStatement(JSContext* cx);

MOZ_MUST_USE bool EnterWith(JSContext* cx, BaselineFrame* frame,
                            HandleValue val, Handle<WithScope*> templ);
MOZ_MUST_USE bool LeaveWith(JSContext* cx, BaselineFrame* frame);

MOZ_MUST_USE bool PushLexicalEnv(JSContext* cx, BaselineFrame* frame,
                                 Handle<LexicalScope*> scope);
MOZ_MUST_USE bool PopLexicalEnv(JSContext* cx, BaselineFrame* frame);
MOZ_MUST_USE bool DebugLeaveThenPopLexicalEnv(JSContext* cx,
                                              BaselineFrame* frame,
                                              jsbytecode* pc);
MOZ_MUST_USE bool FreshenLexicalEnv(JSContext* cx, BaselineFrame* frame);
MOZ_MUST_USE bool DebugLeaveThenFreshenLexicalEnv(JSContext* cx,
                                                  BaselineFrame* frame,
                                                  jsbytecode* pc);
MOZ_MUST_USE bool RecreateLexicalEnv(JSContext* cx, BaselineFrame* frame);
MOZ_MUST_USE bool DebugLeaveThenRecreateLexicalEnv(JSContext* cx,
                                                   BaselineFrame* frame,
                                                   jsbytecode* pc);
MOZ_MUST_USE bool DebugLeaveLexicalEnv(JSContext* cx, BaselineFrame* frame,
                                       jsbytecode* pc);

MOZ_MUST_USE bool PushVarEnv(JSContext* cx, BaselineFrame* frame,
                             HandleScope scope);
MOZ_MUST_USE bool PopVarEnv(JSContext* cx, BaselineFrame* frame);

MOZ_MUST_USE bool InitBaselineFrameForOsr(BaselineFrame* frame,
                                          InterpreterFrame* interpFrame,
                                          uint32_t numStackValues);

JSObject* CreateDerivedTypedObj(JSContext* cx, HandleObject descr,
                                HandleObject owner, int32_t offset);

MOZ_MUST_USE bool IonRecompile(JSContext* cx);
MOZ_MUST_USE bool IonForcedRecompile(JSContext* cx);
MOZ_MUST_USE bool IonForcedInvalidation(JSContext* cx);

JSString* StringReplace(JSContext* cx, HandleString string,
                        HandleString pattern, HandleString repl);

MOZ_MUST_USE bool SetDenseElement(JSContext* cx, HandleNativeObject obj,
                                  int32_t index, HandleValue value,
                                  bool strict);

void AssertValidObjectPtr(JSContext* cx, JSObject* obj);
void AssertValidObjectOrNullPtr(JSContext* cx, JSObject* obj);
void AssertValidStringPtr(JSContext* cx, JSString* str);
void AssertValidSymbolPtr(JSContext* cx, JS::Symbol* sym);
void AssertValidBigIntPtr(JSContext* cx, JS::BigInt* bi);
void AssertValidValue(JSContext* cx, Value* v);

void MarkValueFromJit(JSRuntime* rt, Value* vp);
void MarkStringFromJit(JSRuntime* rt, JSString** stringp);
void MarkObjectFromJit(JSRuntime* rt, JSObject** objp);
void MarkShapeFromJit(JSRuntime* rt, Shape** shapep);
void MarkObjectGroupFromJit(JSRuntime* rt, ObjectGroup** groupp);

// Helper for generatePreBarrier.
inline void* JitMarkFunction(MIRType type) {
  switch (type) {
    case MIRType::Value:
      return JS_FUNC_TO_DATA_PTR(void*, MarkValueFromJit);
    case MIRType::String:
      return JS_FUNC_TO_DATA_PTR(void*, MarkStringFromJit);
    case MIRType::Object:
      return JS_FUNC_TO_DATA_PTR(void*, MarkObjectFromJit);
    case MIRType::Shape:
      return JS_FUNC_TO_DATA_PTR(void*, MarkShapeFromJit);
    case MIRType::ObjectGroup:
      return JS_FUNC_TO_DATA_PTR(void*, MarkObjectGroupFromJit);
    default:
      MOZ_CRASH();
  }
}

bool ObjectIsCallable(JSObject* obj);
bool ObjectIsConstructor(JSObject* obj);

MOZ_MUST_USE bool ThrowRuntimeLexicalError(JSContext* cx, unsigned errorNumber);

MOZ_MUST_USE bool BaselineThrowUninitializedThis(JSContext* cx,
                                                 BaselineFrame* frame);

MOZ_MUST_USE bool BaselineThrowInitializedThis(JSContext* cx);

MOZ_MUST_USE bool ThrowBadDerivedReturn(JSContext* cx, HandleValue v);

MOZ_MUST_USE bool ThrowObjectCoercible(JSContext* cx, HandleValue v);

MOZ_MUST_USE bool BaselineGetFunctionThis(JSContext* cx, BaselineFrame* frame,
                                          MutableHandleValue res);

MOZ_MUST_USE bool CallNativeGetter(JSContext* cx, HandleFunction callee,
                                   HandleObject obj, MutableHandleValue result);

MOZ_MUST_USE bool CallNativeGetterByValue(JSContext* cx, HandleFunction callee,
                                          HandleValue receiver,
                                          MutableHandleValue result);

MOZ_MUST_USE bool CallNativeSetter(JSContext* cx, HandleFunction callee,
                                   HandleObject obj, HandleValue rhs);

MOZ_MUST_USE bool EqualStringsHelperPure(JSString* str1, JSString* str2);

MOZ_MUST_USE bool CheckIsCallable(JSContext* cx, HandleValue v,
                                  CheckIsCallableKind kind);

void HandleCodeCoverageAtPC(BaselineFrame* frame, jsbytecode* pc);
void HandleCodeCoverageAtPrologue(BaselineFrame* frame);

template <bool HandleMissing>
bool GetNativeDataPropertyPure(JSContext* cx, JSObject* obj, PropertyName* name,
                               Value* vp);

template <bool HandleMissing>
bool GetNativeDataPropertyByValuePure(JSContext* cx, JSObject* obj, Value* vp);

template <bool HasOwn>
bool HasNativeDataPropertyPure(JSContext* cx, JSObject* obj, Value* vp);

bool HasNativeElementPure(JSContext* cx, NativeObject* obj, int32_t index,
                          Value* vp);

template <bool NeedsTypeBarrier>
bool SetNativeDataPropertyPure(JSContext* cx, JSObject* obj, PropertyName* name,
                               Value* val);

bool ObjectHasGetterSetterPure(JSContext* cx, JSObject* obj, Shape* propShape);

JSString* TypeOfObject(JSObject* obj, JSRuntime* rt);

bool GetPrototypeOf(JSContext* cx, HandleObject target,
                    MutableHandleValue rval);

bool DoConcatStringObject(JSContext* cx, HandleValue lhs, HandleValue rhs,
                          MutableHandleValue res);

// Wrapper for js::TrySkipAwait.
// If the await operation can be skipped and the resolution value for `val` can
// be acquired, stored the resolved value to `resolved`.  Otherwise, stores
// the JS_CANNOT_SKIP_AWAIT magic value to `resolved`.
MOZ_MUST_USE bool TrySkipAwait(JSContext* cx, HandleValue val,
                               MutableHandleValue resolved);

bool IsPossiblyWrappedTypedArray(JSContext* cx, JSObject* obj, bool* result);

bool DoToNumber(JSContext* cx, HandleValue arg, MutableHandleValue ret);
bool DoToNumeric(JSContext* cx, HandleValue arg, MutableHandleValue ret);

enum class TailCallVMFunctionId;
enum class VMFunctionId;

extern const VMFunctionData& GetVMFunction(VMFunctionId id);
extern const VMFunctionData& GetVMFunction(TailCallVMFunctionId id);

}  // namespace jit
}  // namespace js

#endif /* jit_VMFunctions_h */
