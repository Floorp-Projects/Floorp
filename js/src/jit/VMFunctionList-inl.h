/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/Eval.h"
#include "builtin/RegExp.h"
#include "jit/BaselineIC.h"
#include "jit/IonIC.h"
#include "jit/JitRealm.h"
#include "jit/VMFunctions.h"
#include "vm/AsyncFunction.h"
#include "vm/AsyncIteration.h"
#include "vm/EqualityOperations.h"
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
  _(AddValues, js::AddValues)                                                  \
  _(ArgumentsObjectCreateForIon, js::ArgumentsObject::createForIon)            \
  _(ArrayConstructorOneArg, js::ArrayConstructorOneArg)                        \
  _(ArrayJoin, js::jit::ArrayJoin)                                             \
  _(ArrayPopDense, js::jit::ArrayPopDense)                                     \
  _(ArrayPushDense, js::jit::ArrayPushDense)                                   \
  _(ArrayShiftDense, js::jit::ArrayShiftDense)                                 \
  _(ArraySliceDense, js::ArraySliceDense)                                      \
  _(AsyncFunctionAwait, js::AsyncFunctionAwait)                                \
  _(AsyncFunctionResolve, js::AsyncFunctionResolve)                            \
  _(BaselineDebugPrologue, js::jit::DebugPrologue)                             \
  _(BaselineGetFunctionThis, js::jit::BaselineGetFunctionThis)                 \
  _(BaselineThrowInitializedThis, js::jit::BaselineThrowInitializedThis)       \
  _(BaselineThrowUninitializedThis, js::jit::BaselineThrowUninitializedThis)   \
  _(BindVarOperation, js::BindVarOperation)                                    \
  _(BitAnd, js::BitAnd)                                                        \
  _(BitLsh, js::BitLsh)                                                        \
  _(BitNot, js::BitNot)                                                        \
  _(BitOr, js::BitOr)                                                          \
  _(BitRsh, js::BitRsh)                                                        \
  _(BitXor, js::BitXor)                                                        \
  _(BoxNonStrictThis, js::BoxNonStrictThis)                                    \
  _(BuiltinProtoOperation, js::BuiltinProtoOperation)                          \
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
  _(CreateDerivedTypedObj, js::jit::CreateDerivedTypedObj)                     \
  _(CreateGenerator, js::jit::CreateGenerator)                                 \
  _(CreateThis, js::jit::CreateThis)                                           \
  _(CreateThisForFunctionWithProto, js::CreateThisForFunctionWithProto)        \
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
  _(DeleteElementNonStrict, js::DeleteElementJit<false>)                       \
  _(DeleteElementStrict, js::DeleteElementJit<true>)                           \
  _(DeleteNameOperation, js::DeleteNameOperation)                              \
  _(DeletePropertyNonStrict, js::DeletePropertyJit<false>)                     \
  _(DeletePropertyStrict, js::DeletePropertyJit<true>)                         \
  _(DirectEvalStringFromIon, js::DirectEvalStringFromIon)                      \
  _(DivValues, js::DivValues)                                                  \
  _(DoToNumber, js::jit::DoToNumber)                                           \
  _(DoToNumeric, js::jit::DoToNumeric)                                         \
  _(EnterWith, js::jit::EnterWith)                                             \
  _(FinalSuspend, js::jit::FinalSuspend)                                       \
  _(FinishBoundFunctionInit, JSFunction::finishBoundFunctionInit)              \
  _(FreshenLexicalEnv, js::jit::FreshenLexicalEnv)                             \
  _(FunWithProtoOperation, js::FunWithProtoOperation)                          \
  _(GetAndClearException, js::GetAndClearException)                            \
  _(GetElementOperation, js::GetElementOperation)                              \
  _(GetFirstDollarIndexRaw, js::GetFirstDollarIndexRaw)                        \
  _(GetImportOperation, js::GetImportOperation)                                \
  _(GetIntrinsicValue, js::jit::GetIntrinsicValue)                             \
  _(GetNonSyntacticGlobalThis, js::GetNonSyntacticGlobalThis)                  \
  _(GetOrCreateModuleMetaObject, js::GetOrCreateModuleMetaObject)              \
  _(GetPrototypeOf, js::jit::GetPrototypeOf)                                   \
  _(GetValueProperty, js::GetValueProperty)                                    \
  _(GlobalNameConflictsCheckFromIon, js::jit::GlobalNameConflictsCheckFromIon) \
  _(GreaterThan, js::jit::GreaterThan)                                         \
  _(GreaterThanOrEqual, js::jit::GreaterThanOrEqual)                           \
  _(HomeObjectSuperBase, js::HomeObjectSuperBase)                              \
  _(ImplicitThisOperation, js::ImplicitThisOperation)                          \
  _(ImportMetaOperation, js::ImportMetaOperation)                              \
  _(InitElemGetterSetterOperation, js::InitElemGetterSetterOperation)          \
  _(InitElemOperation, js::InitElemOperation)                                  \
  _(InitElementArray, js::InitElementArray)                                    \
  _(InitFunctionEnvironmentObjects, js::jit::InitFunctionEnvironmentObjects)   \
  _(InitPropGetterSetterOperation, js::InitPropGetterSetterOperation)          \
  _(InitRestParameter, js::jit::InitRestParameter)                             \
  _(InlineTypedObjectCreateCopy, js::InlineTypedObject::createCopy)            \
  _(Int32ToString, js::Int32ToString<CanGC>)                                   \
  _(InterpretResume, js::jit::InterpretResume)                                 \
  _(InterruptCheck, js::jit::InterruptCheck)                                   \
  _(InvokeFunction, js::jit::InvokeFunction)                                   \
  _(InvokeFunctionShuffleNewTarget, js::jit::InvokeFunctionShuffleNewTarget)   \
  _(IonBinaryArithICUpdate, js::jit::IonBinaryArithIC::update)                 \
  _(IonBindNameICUpdate, js::jit::IonBindNameIC::update)                       \
  _(IonCompareICUpdate, js::jit::IonCompareIC::update)                         \
  _(IonCompileScriptForBaseline, js::jit::IonCompileScriptForBaseline)         \
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
  _(LessThan, js::jit::LessThan)                                               \
  _(LessThanOrEqual, js::jit::LessThanOrEqual)                                 \
  _(LexicalEnvironmentObjectCreate, js::LexicalEnvironmentObject::create)      \
  _(LooselyEqual, js::jit::LooselyEqual<true>)                                 \
  _(LooselyNotEqual, js::jit::LooselyEqual<false>)                             \
  _(MakeDefaultConstructor, js::MakeDefaultConstructor)                        \
  _(ModValues, js::ModValues)                                                  \
  _(MulValues, js::MulValues)                                                  \
  _(MutatePrototype, js::jit::MutatePrototype)                                 \
  _(NamedLambdaObjectCreateTemplateObject,                                     \
    js::NamedLambdaObject::createTemplateObject)                               \
  _(NewArgumentsObject, js::jit::NewArgumentsObject)                           \
  _(NewArrayCopyOnWriteOperation, js::NewArrayCopyOnWriteOperation)            \
  _(NewArrayIteratorObject, js::NewArrayIteratorObject)                        \
  _(NewArrayOperation, js::NewArrayOperation)                                  \
  _(NewArrayWithGroup, js::NewArrayWithGroup)                                  \
  _(NewCallObject, js::jit::NewCallObject)                                     \
  _(NewDenseCopyOnWriteArray, js::NewDenseCopyOnWriteArray)                    \
  _(NewObjectOperation, js::NewObjectOperation)                                \
  _(NewObjectOperationWithTemplate, js::NewObjectOperationWithTemplate)        \
  _(NewRegExpStringIteratorObject, js::NewRegExpStringIteratorObject)          \
  _(NewStringIteratorObject, js::NewStringIteratorObject)                      \
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
  _(PopVarEnv, js::jit::PopVarEnv)                                             \
  _(PowValues, js::PowValues)                                                  \
  _(ProcessCallSiteObjOperation, js::ProcessCallSiteObjOperation)              \
  _(PushLexicalEnv, js::jit::PushLexicalEnv)                                   \
  _(PushVarEnv, js::jit::PushVarEnv)                                           \
  _(RecreateLexicalEnv, js::jit::RecreateLexicalEnv)                           \
  _(RegExpMatcherRaw, js::RegExpMatcherRaw)                                    \
  _(RegExpSearcherRaw, js::RegExpSearcherRaw)                                  \
  _(RegExpTesterRaw, js::RegExpTesterRaw)                                      \
  _(SameValue, js::SameValue)                                                  \
  _(SetDenseElement, js::jit::SetDenseElement)                                 \
  _(SetFunctionName, js::SetFunctionName)                                      \
  _(SetIntrinsicOperation, js::SetIntrinsicOperation)                          \
  _(SetObjectElementWithReceiver, js::SetObjectElementWithReceiver)            \
  _(SetProperty, js::jit::SetProperty)                                         \
  _(SetPropertySuper, js::SetPropertySuper)                                    \
  _(SingletonObjectLiteralOperation, js::SingletonObjectLiteralOperation)      \
  _(StartDynamicModuleImport, js::StartDynamicModuleImport)                    \
  _(StrictlyEqual, js::jit::StrictlyEqual<true>)                               \
  _(StrictlyNotEqual, js::jit::StrictlyEqual<false>)                           \
  _(StringFlatReplaceString, js::StringFlatReplaceString)                      \
  _(StringFromCharCode, js::jit::StringFromCharCode)                           \
  _(StringFromCodePoint, js::jit::StringFromCodePoint)                         \
  _(StringReplace, js::jit::StringReplace)                                     \
  _(StringSplitString, js::StringSplitString)                                  \
  _(StringToLowerCase, js::StringToLowerCase)                                  \
  _(StringToNumber, js::StringToNumber)                                        \
  _(StringToUpperCase, js::StringToUpperCase)                                  \
  _(StringsEqual, js::jit::StringsEqual<true>)                                 \
  _(StringsNotEqual, js::jit::StringsEqual<false>)                             \
  _(SubValues, js::SubValues)                                                  \
  _(SubstringKernel, js::SubstringKernel)                                      \
  _(SuperFunOperation, js::SuperFunOperation)                                  \
  _(ThrowBadDerivedReturn, js::jit::ThrowBadDerivedReturn)                     \
  _(ThrowCheckIsObject, js::ThrowCheckIsObject)                                \
  _(ThrowMsgOperation, js::ThrowMsgOperation)                                  \
  _(ThrowObjectCoercible, js::jit::ThrowObjectCoercible)                       \
  _(ThrowOperation, js::ThrowOperation)                                        \
  _(ThrowRuntimeLexicalError, js::jit::ThrowRuntimeLexicalError)               \
  _(ToIdOperation, js::ToIdOperation)                                          \
  _(ToObjectSlow, js::ToObjectSlow)                                            \
  _(ToStringSlow, js::ToStringSlow<CanGC>)                                     \
  _(TrySkipAwait, js::jit::TrySkipAwait)                                       \
  _(UnboxedPlainObjectConvertToNative,                                         \
    js::UnboxedPlainObject::convertToNative)                                   \
  _(UrshValues, js::UrshValues)

enum class VMFunctionId {
#define DEF_ID(name, fp) name,
  VMFUNCTION_LIST(DEF_ID)
#undef DEF_ID
      Count
};

// Define the VMFunctionToId template to map from signature + function to
// the VMFunctionId. This lets us verify the consumer/codegen code matches
// the C++ signature.
template <typename Function, Function fun>
struct VMFunctionToId;  // Error on this line? Forgot to update VMFUNCTION_LIST?

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

#if MOZ_IS_GCC
#  pragma GCC diagnostic pop
#endif

}  // namespace jit
}  // namespace js
