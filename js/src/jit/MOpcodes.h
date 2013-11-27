/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_MOpcodes_h
#define jit_MOpcodes_h

namespace js {
namespace jit {

#define MIR_OPCODE_LIST(_)                                                  \
    _(Constant)                                                             \
    _(Parameter)                                                            \
    _(Callee)                                                               \
    _(TableSwitch)                                                          \
    _(Goto)                                                                 \
    _(Test)                                                                 \
    _(TypeObjectDispatch)                                                   \
    _(FunctionDispatch)                                                     \
    _(Compare)                                                              \
    _(Phi)                                                                  \
    _(Beta)                                                                 \
    _(OsrValue)                                                             \
    _(OsrScopeChain)                                                        \
    _(OsrReturnValue)                                                       \
    _(OsrArgumentsObject)                                                   \
    _(ReturnFromCtor)                                                       \
    _(CheckOverRecursed)                                                    \
    _(DefVar)                                                               \
    _(DefFun)                                                               \
    _(CreateThis)                                                           \
    _(CreateThisWithProto)                                                  \
    _(CreateThisWithTemplate)                                               \
    _(CreateArgumentsObject)                                                \
    _(GetArgumentsObjectArg)                                                \
    _(SetArgumentsObjectArg)                                                \
    _(ComputeThis)                                                          \
    _(PrepareCall)                                                          \
    _(PassArg)                                                              \
    _(Call)                                                                 \
    _(ApplyArgs)                                                            \
    _(Bail)                                                                 \
    _(AssertFloat32)                                                        \
    _(GetDynamicName)                                                       \
    _(FilterArgumentsOrEval)                                                \
    _(CallDirectEval)                                                       \
    _(BitNot)                                                               \
    _(TypeOf)                                                               \
    _(ToId)                                                                 \
    _(BitAnd)                                                               \
    _(BitOr)                                                                \
    _(BitXor)                                                               \
    _(Lsh)                                                                  \
    _(Rsh)                                                                  \
    _(Ursh)                                                                 \
    _(MinMax)                                                               \
    _(Abs)                                                                  \
    _(Sqrt)                                                                 \
    _(Atan2)                                                                \
    _(Hypot)                                                                \
    _(Pow)                                                                  \
    _(PowHalf)                                                              \
    _(Random)                                                               \
    _(MathFunction)                                                         \
    _(Add)                                                                  \
    _(Sub)                                                                  \
    _(Mul)                                                                  \
    _(Div)                                                                  \
    _(Mod)                                                                  \
    _(Concat)                                                               \
    _(ConcatPar)                                                            \
    _(CharCodeAt)                                                           \
    _(FromCharCode)                                                         \
    _(StringSplit)                                                          \
    _(Return)                                                               \
    _(Throw)                                                                \
    _(Box)                                                                  \
    _(Unbox)                                                                \
    _(GuardObject)                                                          \
    _(GuardString)                                                          \
    _(AssertRange)                                                          \
    _(ToDouble)                                                             \
    _(ToFloat32)                                                            \
    _(ToInt32)                                                              \
    _(TruncateToInt32)                                                      \
    _(ToString)                                                             \
    _(NewSlots)                                                             \
    _(NewParallelArray)                                                     \
    _(NewArray)                                                             \
    _(NewObject)                                                            \
    _(NewDeclEnvObject)                                                     \
    _(NewCallObject)                                                        \
    _(NewStringObject)                                                      \
    _(InitElem)                                                             \
    _(InitElemGetterSetter)                                                 \
    _(InitProp)                                                             \
    _(InitPropGetterSetter)                                                 \
    _(Start)                                                                \
    _(OsrEntry)                                                             \
    _(Nop)                                                                  \
    _(RegExp)                                                               \
    _(RegExpTest)                                                           \
    _(Lambda)                                                               \
    _(ImplicitThis)                                                         \
    _(Slots)                                                                \
    _(Elements)                                                             \
    _(ConstantElements)                                                     \
    _(ConvertElementsToDoubles)                                             \
    _(MaybeToDoubleElement)                                                 \
    _(LoadSlot)                                                             \
    _(StoreSlot)                                                            \
    _(FunctionEnvironment)                                                  \
    _(TypeBarrier)                                                          \
    _(MonitorTypes)                                                         \
    _(PostWriteBarrier)                                                     \
    _(GetPropertyCache)                                                     \
    _(GetPropertyPolymorphic)                                               \
    _(SetPropertyPolymorphic)                                               \
    _(GetElementCache)                                                      \
    _(SetElementCache)                                                      \
    _(BindNameCache)                                                        \
    _(GuardShape)                                                           \
    _(GuardObjectType)                                                      \
    _(GuardObjectIdentity)                                                  \
    _(GuardClass)                                                           \
    _(ArrayLength)                                                          \
    _(TypedArrayLength)                                                     \
    _(TypedArrayElements)                                                   \
    _(TypedObjectElements)                                                  \
    _(InitializedLength)                                                    \
    _(SetInitializedLength)                                                 \
    _(Not)                                                                  \
    _(BoundsCheck)                                                          \
    _(BoundsCheckLower)                                                     \
    _(InArray)                                                              \
    _(LoadElement)                                                          \
    _(LoadElementHole)                                                      \
    _(StoreElement)                                                         \
    _(StoreElementHole)                                                     \
    _(ArrayPopShift)                                                        \
    _(ArrayPush)                                                            \
    _(ArrayConcat)                                                          \
    _(LoadTypedArrayElement)                                                \
    _(LoadTypedArrayElementHole)                                            \
    _(LoadTypedArrayElementStatic)                                          \
    _(StoreTypedArrayElement)                                               \
    _(StoreTypedArrayElementHole)                                           \
    _(StoreTypedArrayElementStatic)                                         \
    _(EffectiveAddress)                                                     \
    _(ClampToUint8)                                                         \
    _(LoadFixedSlot)                                                        \
    _(StoreFixedSlot)                                                       \
    _(CallGetProperty)                                                      \
    _(GetNameCache)                                                         \
    _(CallGetIntrinsicValue)                                                \
    _(CallsiteCloneCache)                                                   \
    _(CallGetElement)                                                       \
    _(CallSetElement)                                                       \
    _(CallSetProperty)                                                      \
    _(CallInitElementArray)                                                 \
    _(DeleteProperty)                                                       \
    _(DeleteElement)                                                        \
    _(SetPropertyCache)                                                     \
    _(IteratorStart)                                                        \
    _(IteratorNext)                                                         \
    _(IteratorMore)                                                         \
    _(IteratorEnd)                                                          \
    _(StringLength)                                                         \
    _(ArgumentsLength)                                                      \
    _(GetFrameArgument)                                                     \
    _(SetFrameArgument)                                                     \
    _(RunOncePrologue)                                                      \
    _(Rest)                                                                 \
    _(Floor)                                                                \
    _(Round)                                                                \
    _(In)                                                                   \
    _(InstanceOf)                                                           \
    _(CallInstanceOf)                                                       \
    _(InterruptCheck)                                                       \
    _(FunctionBoundary)                                                     \
    _(GetDOMProperty)                                                       \
    _(GetDOMMember)                                                         \
    _(SetDOMProperty)                                                       \
    _(IsCallable)                                                           \
    _(HaveSameClass)                                                        \
    _(AsmJSNeg)                                                             \
    _(AsmJSUnsignedToDouble)                                                \
    _(AsmJSUnsignedToFloat32)                                               \
    _(AsmJSLoadHeap)                                                        \
    _(AsmJSStoreHeap)                                                       \
    _(AsmJSLoadGlobalVar)                                                   \
    _(AsmJSStoreGlobalVar)                                                  \
    _(AsmJSLoadFuncPtr)                                                     \
    _(AsmJSLoadFFIFunc)                                                     \
    _(AsmJSReturn)                                                          \
    _(AsmJSParameter)                                                       \
    _(AsmJSVoidReturn)                                                      \
    _(AsmJSPassStackArg)                                                    \
    _(AsmJSCall)                                                            \
    _(AsmJSCheckOverRecursed)                                               \
    _(CheckOverRecursedPar)                                                 \
    _(NewCallObjectPar)                                                     \
    _(NewPar)                                                               \
    _(NewDenseArrayPar)                                                     \
    _(NewDerivedTypedObject)                                                \
    _(AbortPar)                                                             \
    _(LambdaPar)                                                            \
    _(RestPar)                                                              \
    _(ForkJoinSlice)                                                        \
    _(GuardThreadLocalObject)                                               \
    _(CheckInterruptPar)

// Forward declarations of MIR types.
#define FORWARD_DECLARE(op) class M##op;
 MIR_OPCODE_LIST(FORWARD_DECLARE)
#undef FORWARD_DECLARE

class MInstructionVisitor // interface i.e. pure abstract class
{
  public:
#define VISIT_INS(op) virtual bool visit##op(M##op *) = 0;
    MIR_OPCODE_LIST(VISIT_INS)
#undef VISIT_INS
};

class MInstructionVisitorWithDefaults : public MInstructionVisitor
{
  public:
#define VISIT_INS(op) virtual bool visit##op(M##op *) { MOZ_ASSUME_UNREACHABLE("NYI: " #op); }
    MIR_OPCODE_LIST(VISIT_INS)
#undef VISIT_INS
};

} // namespace jit
} // namespace js

#endif /* jit_MOpcodes_h */
