/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_VMFunctionList_inl_h
#define jit_VMFunctionList_inl_h

#include "builtin/Eval.h"
#include "builtin/Promise.h"  // js::AsyncFunctionAwait
#include "builtin/RegExp.h"
#include "builtin/String.h"
#include "jit/BaselineIC.h"
#include "jit/IonIC.h"
#include "jit/JitRealm.h"
#include "jit/VMFunctions.h"
#include "vm/AsyncFunction.h"
#include "vm/AsyncIteration.h"
#include "vm/BigIntType.h"
#include "vm/EqualityOperations.h"
#include "vm/Instrumentation.h"
#include "vm/Interpreter.h"
#include "vm/TypedArrayObject.h"

#include "jit/BaselineFrame-inl.h"
#include "vm/Interpreter-inl.h"

namespace js {
namespace jit {

// List of all VM functions to be used with callVM. Each entry stores the name
// (must be unique, used for the VMFunctionId enum and profiling) and the C++
// function to be called. This list must be sorted on the name field.
#define VMFUNCTION_LIST(_)                                                     \
  _(AddOrUpdateSparseElementHelper, js::AddOrUpdateSparseElementHelper)        \
  _(ArgumentsObjectCreateForIon, js::ArgumentsObject::createForIon)            \
  _(ArrayConstructorOneArg, js::ArrayConstructorOneArg)                        \
  _(ArrayJoin, js::jit::ArrayJoin)                                             \
  _(ArrayPopDense, js::jit::ArrayPopDense)                                     \
  _(ArrayPushDense, js::jit::ArrayPushDense)                                   \
  _(ArrayShiftDense, js::jit::ArrayShiftDense)                                 \
  _(ArraySliceDense, js::ArraySliceDense)                                      \
  _(AsyncFunctionAwait, js::AsyncFunctionAwait)                                \
  _(AsyncFunctionResolve, js::AsyncFunctionResolve)                            \
  _(BaselineCompileFromBaselineInterpreter,                                    \
    js::jit::BaselineCompileFromBaselineInterpreter)                           \
  _(BaselineDebugPrologue, js::jit::DebugPrologue)                             \
  _(BaselineGetFunctionThis, js::jit::BaselineGetFunctionThis)                 \
  _(BigIntAdd, JS::BigInt::add)                                                \
  _(BigIntBitAnd, JS::BigInt::bitAnd)                                          \
  _(BigIntBitNot, JS::BigInt::bitNot)                                          \
  _(BigIntBitOr, JS::BigInt::bitOr)                                            \
  _(BigIntBitXor, JS::BigInt::bitXor)                                          \
  _(BigIntDec, JS::BigInt::dec)                                                \
  _(BigIntDiv, JS::BigInt::div)                                                \
  _(BigIntInc, JS::BigInt::inc)                                                \
  _(BigIntLeftShift, JS::BigInt::lsh)                                          \
  _(BigIntMod, JS::BigInt::mod)                                                \
  _(BigIntMul, JS::BigInt::mul)                                                \
  _(BigIntNeg, JS::BigInt::neg)                                                \
  _(BigIntPow, JS::BigInt::pow)                                                \
  _(BigIntRightShift, JS::BigInt::rsh)                                         \
  _(BigIntStringEqual,                                                         \
    js::jit::BigIntStringEqual<js::jit::EqualityKind::Equal>)                  \
  _(BigIntStringGreaterThanOrEqual,                                            \
    js::jit::BigIntStringCompare<js::jit::ComparisonKind::GreaterThanOrEqual>) \
  _(BigIntStringLessThan,                                                      \
    js::jit::BigIntStringCompare<js::jit::ComparisonKind::LessThan>)           \
  _(BigIntStringNotEqual,                                                      \
    js::jit::BigIntStringEqual<js::jit::EqualityKind::NotEqual>)               \
  _(BigIntSub, JS::BigInt::sub)                                                \
  _(BindVarOperation, js::BindVarOperation)                                    \
  _(BoxBoxableValue, js::wasm::BoxBoxableValue)                                \
  _(BoxNonStrictThis, js::BoxNonStrictThis)                                    \
  _(CallNativeGetter, js::jit::CallNativeGetter)                               \
  _(CallNativeGetterByValue, js::jit::CallNativeGetterByValue)                 \
  _(CallNativeSetter, js::jit::CallNativeSetter)                               \
  _(CharCodeAt, js::jit::CharCodeAt)                                           \
  _(CheckClassHeritageOperation, js::CheckClassHeritageOperation)              \
  _(CheckGlobalOrEvalDeclarationConflicts,                                     \
    js::CheckGlobalOrEvalDeclarationConflicts)                                 \
  _(CheckIsCallable, js::jit::CheckIsCallable)                                 \
  _(CheckOverRecursed, js::jit::CheckOverRecursed)                             \
  _(CheckOverRecursedBaseline, js::jit::CheckOverRecursedBaseline)             \
  _(CloneRegExpObject, js::CloneRegExpObject)                                  \
  _(ConcatStrings, js::ConcatStrings<CanGC>)                                   \
  _(ConvertElementsToDoubles, js::ObjectElements::ConvertElementsToDoubles)    \
  _(CopyElementsForWrite, js::NativeObject::CopyElementsForWrite)              \
  _(CopyLexicalEnvironmentObject, js::jit::CopyLexicalEnvironmentObject)       \
  _(CreateAsyncFromSyncIterator, js::CreateAsyncFromSyncIterator)              \
  _(CreateGenerator, js::jit::CreateGenerator)                                 \
  _(CreateThisForFunctionWithProto, js::CreateThisForFunctionWithProto)        \
  _(CreateThisFromIC, js::jit::CreateThisFromIC)                               \
  _(CreateThisFromIon, js::jit::CreateThisFromIon)                             \
  _(CreateThisWithTemplate, js::CreateThisWithTemplate)                        \
  _(DebugAfterYield, js::jit::DebugAfterYield)                                 \
  _(DebugEpilogueOnBaselineReturn, js::jit::DebugEpilogueOnBaselineReturn)     \
  _(DebugLeaveLexicalEnv, js::jit::DebugLeaveLexicalEnv)                       \
  _(DebugLeaveThenFreshenLexicalEnv, js::jit::DebugLeaveThenFreshenLexicalEnv) \
  _(DebugLeaveThenPopLexicalEnv, js::jit::DebugLeaveThenPopLexicalEnv)         \
  _(DebugLeaveThenRecreateLexicalEnv,                                          \
    js::jit::DebugLeaveThenRecreateLexicalEnv)                                 \
  _(Debug_CheckSelfHosted, js::Debug_CheckSelfHosted)                          \
  _(DeepCloneObjectLiteral, js::DeepCloneObjectLiteral)                        \
  _(DefFunOperation, js::DefFunOperation)                                      \
  _(DefLexicalOperation, js::DefLexicalOperation)                              \
  _(DefVarOperation, js::DefVarOperation)                                      \
  _(DelElemOperationNonStrict, js::DelElemOperation<false>)                    \
  _(DelElemOperationStrict, js::DelElemOperation<true>)                        \
  _(DelPropOperationNonStrict, js::DelPropOperation<false>)                    \
  _(DelPropOperationStrict, js::DelPropOperation<true>)                        \
  _(DeleteNameOperation, js::DeleteNameOperation)                              \
  _(DirectEvalStringFromIon, js::DirectEvalStringFromIon)                      \
  _(DoCallFallback, js::jit::DoCallFallback)                                   \
  _(DoConcatStringObject, js::jit::DoConcatStringObject)                       \
  _(DoSpreadCallFallback, js::jit::DoSpreadCallFallback)                       \
  _(DoToNumeric, js::jit::DoToNumeric)                                         \
  _(DoTypeUpdateFallback, js::jit::DoTypeUpdateFallback)                       \
  _(EnterWith, js::jit::EnterWith)                                             \
  _(FinalSuspend, js::jit::FinalSuspend)                                       \
  _(FinishBoundFunctionInit, JSFunction::finishBoundFunctionInit)              \
  _(FreshenLexicalEnv, js::jit::FreshenLexicalEnv)                             \
  _(FunWithProtoOperation, js::FunWithProtoOperation)                          \
  _(FunctionProtoOperation, js::FunctionProtoOperation)                        \
  _(GeneratorThrowOrReturn, js::jit::GeneratorThrowOrReturn)                   \
  _(GetAndClearException, js::GetAndClearException)                            \
  _(GetElementOperation, js::GetElementOperation)                              \
  _(GetFirstDollarIndexRaw, js::GetFirstDollarIndexRaw)                        \
  _(GetImportOperation, js::GetImportOperation)                                \
  _(GetIntrinsicValue, js::jit::GetIntrinsicValue)                             \
  _(GetNonSyntacticGlobalThis, js::GetNonSyntacticGlobalThis)                  \
  _(GetOrCreateModuleMetaObject, js::GetOrCreateModuleMetaObject)              \
  _(GetPrototypeOf, js::jit::GetPrototypeOf)                                   \
  _(GetSparseElementHelper, js::GetSparseElementHelper)                        \
  _(GetValueProperty, js::GetValueProperty)                                    \
  _(GlobalNameConflictsCheckFromIon, js::jit::GlobalNameConflictsCheckFromIon) \
  _(GreaterThan, js::GreaterThan)                                              \
  _(GreaterThanOrEqual, js::GreaterThanOrEqual)                                \
  _(HandleDebugTrap, js::jit::HandleDebugTrap)                                 \
  _(ImplicitThisOperation, js::ImplicitThisOperation)                          \
  _(ImportMetaOperation, js::ImportMetaOperation)                              \
  _(InitElemGetterSetterOperation, js::InitElemGetterSetterOperation)          \
  _(InitElemOperation, js::InitElemOperation)                                  \
  _(InitElementArray, js::InitElementArray)                                    \
  _(InitFunctionEnvironmentObjects, js::jit::InitFunctionEnvironmentObjects)   \
  _(InitPropGetterSetterOperation, js::InitPropGetterSetterOperation)          \
  _(InitRestParameter, js::jit::InitRestParameter)                             \
  _(InstrumentationActiveOperation, js::InstrumentationActiveOperation)        \
  _(InstrumentationCallbackOperation, js::InstrumentationCallbackOperation)    \
  _(InstrumentationScriptIdOperation, js::InstrumentationScriptIdOperation)    \
  _(Int32ToString, js::Int32ToString<CanGC>)                                   \
  _(InterpretResume, js::jit::InterpretResume)                                 \
  _(InterruptCheck, js::jit::InterruptCheck)                                   \
  _(InvokeFunction, js::jit::InvokeFunction)                                   \
  _(IonBinaryArithICUpdate, js::jit::IonBinaryArithIC::update)                 \
  _(IonBindNameICUpdate, js::jit::IonBindNameIC::update)                       \
  _(IonCompareICUpdate, js::jit::IonCompareIC::update)                         \
  _(IonCompileScriptForBaselineAtEntry,                                        \
    js::jit::IonCompileScriptForBaselineAtEntry)                               \
  _(IonCompileScriptForBaselineOSR, js::jit::IonCompileScriptForBaselineOSR)   \
  _(IonForcedInvalidation, js::jit::IonForcedInvalidation)                     \
  _(IonForcedRecompile, js::jit::IonForcedRecompile)                           \
  _(IonGetIteratorICUpdate, js::jit::IonGetIteratorIC::update)                 \
  _(IonGetNameICUpdate, js::jit::IonGetNameIC::update)                         \
  _(IonGetPropSuperICUpdate, js::jit::IonGetPropSuperIC::update)               \
  _(IonGetPropertyICUpdate, js::jit::IonGetPropertyIC::update)                 \
  _(IonHasOwnICUpdate, js::jit::IonHasOwnIC::update)                           \
  _(IonInICUpdate, js::jit::IonInIC::update)                                   \
  _(IonInstanceOfICUpdate, js::jit::IonInstanceOfIC::update)                   \
  _(IonRecompile, js::jit::IonRecompile)                                       \
  _(IonSetPropertyICUpdate, js::jit::IonSetPropertyIC::update)                 \
  _(IonUnaryArithICUpdate, js::jit::IonUnaryArithIC::update)                   \
  _(IsArrayFromJit, js::IsArrayFromJit)                                        \
  _(IsPossiblyWrappedTypedArray, js::jit::IsPossiblyWrappedTypedArray)         \
  _(IsPrototypeOf, js::IsPrototypeOf)                                          \
  _(Lambda, js::Lambda)                                                        \
  _(LambdaArrow, js::LambdaArrow)                                              \
  _(LeaveWith, js::jit::LeaveWith)                                             \
  _(LessThan, js::LessThan)                                                    \
  _(LessThanOrEqual, js::LessThanOrEqual)                                      \
  _(LexicalEnvironmentObjectCreate, js::LexicalEnvironmentObject::create)      \
  _(LooselyEqual, js::jit::LooselyEqual<js::jit::EqualityKind::Equal>)         \
  _(LooselyNotEqual, js::jit::LooselyEqual<js::jit::EqualityKind::NotEqual>)   \
  _(MakeDefaultConstructor, js::MakeDefaultConstructor)                        \
  _(MutatePrototype, js::jit::MutatePrototype)                                 \
  _(NamedLambdaObjectCreateTemplateObject,                                     \
    js::NamedLambdaObject::createTemplateObject)                               \
  _(NativeGetElement, js::NativeGetElement)                                    \
  _(NewArgumentsObject, js::jit::NewArgumentsObject)                           \
  _(NewArrayCopyOnWriteOperation, js::NewArrayCopyOnWriteOperation)            \
  _(NewArrayIterator, js::NewArrayIterator)                                    \
  _(NewArrayOperation, js::NewArrayOperation)                                  \
  _(NewArrayWithGroup, js::NewArrayWithGroup)                                  \
  _(NewCallObject, js::jit::NewCallObject)                                     \
  _(NewDenseCopyOnWriteArray, js::NewDenseCopyOnWriteArray)                    \
  _(NewObjectOperation, js::NewObjectOperation)                                \
  _(NewObjectOperationWithTemplate, js::NewObjectOperationWithTemplate)        \
  _(NewRegExpStringIteratorObject, js::NewRegExpStringIteratorObject)          \
  _(NewStringIterator, js::NewStringIterator)                                  \
  _(NewStringObject, js::jit::NewStringObject)                                 \
  _(NewTypedArrayWithTemplateAndArray, js::NewTypedArrayWithTemplateAndArray)  \
  _(NewTypedArrayWithTemplateAndBuffer,                                        \
    js::NewTypedArrayWithTemplateAndBuffer)                                    \
  _(NewTypedArrayWithTemplateAndLength,                                        \
    js::NewTypedArrayWithTemplateAndLength)                                    \
  _(NormalSuspend, js::jit::NormalSuspend)                                     \
  _(NumberToString, js::NumberToString<CanGC>)                                 \
  _(ObjectClassToString, js::ObjectClassToString)                              \
  _(ObjectCreateWithTemplate, js::ObjectCreateWithTemplate)                    \
  _(ObjectWithProtoOperation, js::ObjectWithProtoOperation)                    \
  _(OnDebuggerStatement, js::jit::OnDebuggerStatement)                         \
  _(OperatorInI, js::jit::OperatorInI)                                         \
  _(OptimizeSpreadCall, js::OptimizeSpreadCall)                                \
  _(PopLexicalEnv, js::jit::PopLexicalEnv)                                     \
  _(ProcessCallSiteObjOperation, js::ProcessCallSiteObjOperation)              \
  _(ProxyGetProperty, js::ProxyGetProperty)                                    \
  _(ProxyGetPropertyByValue, js::ProxyGetPropertyByValue)                      \
  _(ProxyHas, js::ProxyHas)                                                    \
  _(ProxyHasOwn, js::ProxyHasOwn)                                              \
  _(ProxySetProperty, js::ProxySetProperty)                                    \
  _(ProxySetPropertyByValue, js::ProxySetPropertyByValue)                      \
  _(PushLexicalEnv, js::jit::PushLexicalEnv)                                   \
  _(PushVarEnv, js::jit::PushVarEnv)                                           \
  _(RecreateLexicalEnv, js::jit::RecreateLexicalEnv)                           \
  _(RegExpMatcherRaw, js::RegExpMatcherRaw)                                    \
  _(RegExpSearcherRaw, js::RegExpSearcherRaw)                                  \
  _(RegExpTesterRaw, js::RegExpTesterRaw)                                      \
  _(SameValue, js::SameValue)                                                  \
  _(SetArrayLength, js::jit::SetArrayLength)                                   \
  _(SetDenseElement, js::jit::SetDenseElement)                                 \
  _(SetFunctionName, js::SetFunctionName)                                      \
  _(SetIntrinsicOperation, js::SetIntrinsicOperation)                          \
  _(SetObjectElementWithReceiver, js::SetObjectElementWithReceiver)            \
  _(SetProperty, js::jit::SetProperty)                                         \
  _(SetPropertySuper, js::SetPropertySuper)                                    \
  _(SingletonObjectLiteralOperation, js::SingletonObjectLiteralOperation)      \
  _(StartDynamicModuleImport, js::StartDynamicModuleImport)                    \
  _(StrictlyEqual, js::jit::StrictlyEqual<js::jit::EqualityKind::Equal>)       \
  _(StrictlyNotEqual, js::jit::StrictlyEqual<js::jit::EqualityKind::NotEqual>) \
  _(StringBigIntGreaterThanOrEqual,                                            \
    js::jit::StringBigIntCompare<js::jit::ComparisonKind::GreaterThanOrEqual>) \
  _(StringBigIntLessThan,                                                      \
    js::jit::StringBigIntCompare<js::jit::ComparisonKind::LessThan>)           \
  _(StringFlatReplaceString, js::StringFlatReplaceString)                      \
  _(StringFromCharCode, js::jit::StringFromCharCode)                           \
  _(StringFromCodePoint, js::jit::StringFromCodePoint)                         \
  _(StringReplace, js::jit::StringReplace)                                     \
  _(StringSplitString, js::StringSplitString)                                  \
  _(StringToLowerCase, js::StringToLowerCase)                                  \
  _(StringToNumber, js::StringToNumber)                                        \
  _(StringToUpperCase, js::StringToUpperCase)                                  \
  _(StringsCompareGreaterThanOrEquals,                                         \
    js::jit::StringsCompare<ComparisonKind::GreaterThanOrEqual>)               \
  _(StringsCompareLessThan, js::jit::StringsCompare<ComparisonKind::LessThan>) \
  _(StringsEqual, js::jit::StringsEqual<js::jit::EqualityKind::Equal>)         \
  _(StringsNotEqual, js::jit::StringsEqual<js::jit::EqualityKind::NotEqual>)   \
  _(SubstringKernel, js::SubstringKernel)                                      \
  _(ThrowBadDerivedReturn, js::jit::ThrowBadDerivedReturn)                     \
  _(ThrowBadDerivedReturnOrUninitializedThis,                                  \
    js::jit::ThrowBadDerivedReturnOrUninitializedThis)                         \
  _(ThrowCheckIsObject, js::ThrowCheckIsObject)                                \
  _(ThrowHomeObjectNotObject, js::ThrowHomeObjectNotObject)                    \
  _(ThrowInitializedThis, js::ThrowInitializedThis)                            \
  _(ThrowMsgOperation, js::ThrowMsgOperation)                                  \
  _(ThrowObjectCoercible, js::ThrowObjectCoercible)                            \
  _(ThrowOperation, js::ThrowOperation)                                        \
  _(ThrowRuntimeLexicalError, js::jit::ThrowRuntimeLexicalError)               \
  _(ThrowUninitializedThis, js::ThrowUninitializedThis)                        \
  _(ToIdOperation, js::ToIdOperation)                                          \
  _(ToObjectSlow, js::ToObjectSlow)                                            \
  _(ToStringSlow, js::ToStringSlow<CanGC>)                                     \
  _(TrySkipAwait, js::jit::TrySkipAwait)

// The list below is for tail calls. The third argument specifies the number of
// non-argument Values the VM wrapper should pop from the stack. This is used
// for Baseline ICs.
#define TAIL_CALL_VMFUNCTION_LIST(_)                            \
  _(DoBinaryArithFallback, js::jit::DoBinaryArithFallback, 2)   \
  _(DoBindNameFallback, js::jit::DoBindNameFallback, 0)         \
  _(DoCompareFallback, js::jit::DoCompareFallback, 2)           \
  _(DoConcatStringObject, js::jit::DoConcatStringObject, 2)     \
  _(DoGetElemFallback, js::jit::DoGetElemFallback, 2)           \
  _(DoGetElemSuperFallback, js::jit::DoGetElemSuperFallback, 3) \
  _(DoGetIntrinsicFallback, js::jit::DoGetIntrinsicFallback, 0) \
  _(DoGetIteratorFallback, js::jit::DoGetIteratorFallback, 1)   \
  _(DoGetNameFallback, js::jit::DoGetNameFallback, 0)           \
  _(DoGetPropFallback, js::jit::DoGetPropFallback, 1)           \
  _(DoGetPropSuperFallback, js::jit::DoGetPropSuperFallback, 0) \
  _(DoHasOwnFallback, js::jit::DoHasOwnFallback, 2)             \
  _(DoInFallback, js::jit::DoInFallback, 2)                     \
  _(DoInstanceOfFallback, js::jit::DoInstanceOfFallback, 2)     \
  _(DoNewArrayFallback, js::jit::DoNewArrayFallback, 0)         \
  _(DoNewObjectFallback, js::jit::DoNewObjectFallback, 0)       \
  _(DoRestFallback, js::jit::DoRestFallback, 0)                 \
  _(DoSetElemFallback, js::jit::DoSetElemFallback, 2)           \
  _(DoSetPropFallback, js::jit::DoSetPropFallback, 1)           \
  _(DoToBoolFallback, js::jit::DoToBoolFallback, 0)             \
  _(DoTypeMonitorFallback, js::jit::DoTypeMonitorFallback, 0)   \
  _(DoTypeOfFallback, js::jit::DoTypeOfFallback, 0)             \
  _(DoUnaryArithFallback, js::jit::DoUnaryArithFallback, 1)

#define DEF_ID(name, ...) name,
enum class VMFunctionId { VMFUNCTION_LIST(DEF_ID) Count };
enum class TailCallVMFunctionId { TAIL_CALL_VMFUNCTION_LIST(DEF_ID) Count };
#undef DEF_ID

// Define the VMFunctionToId template to map from signature + function to
// the VMFunctionId. This lets us verify the consumer/codegen code matches
// the C++ signature.
template <typename Function, Function fun>
struct VMFunctionToId;  // Error here? Update VMFUNCTION_LIST?

template <typename Function, Function fun>
struct TailCallVMFunctionToId;  // Error here? Update TAIL_CALL_VMFUNCTION_LIST?

// GCC warns when the signature does not have matching attributes (for example
// MOZ_MUST_USE). Squelch this warning to avoid a GCC-only footgun.
#if MOZ_IS_GCC
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wignored-attributes"
#endif

// Note: the use of ::fp instead of fp is intentional to enforce use of
// fully-qualified names in the list above.
#define DEF_TEMPLATE(name, fp)                             \
  template <>                                              \
  struct VMFunctionToId<decltype(&(::fp)), ::fp> {         \
    static constexpr VMFunctionId id = VMFunctionId::name; \
  };
VMFUNCTION_LIST(DEF_TEMPLATE)
#undef DEF_TEMPLATE

#define DEF_TEMPLATE(name, fp, valuesToPop)                                \
  template <>                                                              \
  struct TailCallVMFunctionToId<decltype(&(::fp)), ::fp> {                 \
    static constexpr TailCallVMFunctionId id = TailCallVMFunctionId::name; \
  };
TAIL_CALL_VMFUNCTION_LIST(DEF_TEMPLATE)
#undef DEF_TEMPLATE

#if MOZ_IS_GCC
#  pragma GCC diagnostic pop
#endif

}  // namespace jit
}  // namespace js

#endif  // jit_VMFunctionList_inl_h
