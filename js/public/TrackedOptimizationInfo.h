/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_TrackedOptimizationInfo_h
#define js_TrackedOptimizationInfo_h

#include "mozilla/Maybe.h"

namespace JS {

#define TRACKED_STRATEGY_LIST(_)            \
  _(GetProp_ArgumentsLength)                \
  _(GetProp_ArgumentsCallee)                \
  _(GetProp_InferredConstant)               \
  _(GetProp_Constant)                       \
  _(GetProp_NotDefined)                     \
  _(GetProp_StaticName)                     \
  _(GetProp_TypedObject)                    \
  _(GetProp_DefiniteSlot)                   \
  _(GetProp_CommonGetter)                   \
  _(GetProp_InlineAccess)                   \
  _(GetProp_InlineProtoAccess)              \
  _(GetProp_Innerize)                       \
  _(GetProp_InlineCache)                    \
  _(GetProp_ModuleNamespace)                \
                                            \
  _(SetProp_CommonSetter)                   \
  _(SetProp_TypedObject)                    \
  _(SetProp_DefiniteSlot)                   \
  _(SetProp_InlineAccess)                   \
  _(SetProp_InlineCache)                    \
                                            \
  _(GetElem_TypedObject)                    \
  _(GetElem_CallSiteObject)                 \
  _(GetElem_Dense)                          \
  _(GetElem_TypedArray)                     \
  _(GetElem_String)                         \
  _(GetElem_Arguments)                      \
  _(GetElem_ArgumentsInlinedConstant)       \
  _(GetElem_ArgumentsInlinedSwitch)         \
  _(GetElem_InlineCache)                    \
                                            \
  _(SetElem_TypedObject)                    \
  _(SetElem_TypedArray)                     \
  _(SetElem_Dense)                          \
  _(SetElem_Arguments)                      \
  _(SetElem_InlineCache)                    \
                                            \
  _(BinaryArith_Concat)                     \
  _(BinaryArith_SpecializedTypes)           \
  _(BinaryArith_SpecializedOnBaselineTypes) \
  _(BinaryArith_Call)                       \
                                            \
  _(UnaryArith_SpecializedTypes)            \
  _(UnaryArith_SpecializedOnBaselineTypes)  \
  _(UnaryArith_InlineCache)                 \
                                            \
  _(InlineCache_OptimizedStub)              \
                                            \
  _(NewArray_TemplateObject)                \
  _(NewArray_Call)                          \
                                            \
  _(NewObject_TemplateObject)               \
  _(NewObject_Call)                         \
                                            \
  _(Compare_SpecializedTypes)               \
  _(Compare_Bitwise)                        \
  _(Compare_SpecializedOnBaselineTypes)     \
  _(Compare_Call)                           \
  _(Compare_Character)                      \
                                            \
  _(Call_Inline)

// Ordering is important below. All outcomes before GenericSuccess will be
// considered failures, and all outcomes after GenericSuccess will be
// considered successes.
#define TRACKED_OUTCOME_LIST(_)               \
  _(GenericFailure)                           \
  _(Disabled)                                 \
  _(NoTypeInfo)                               \
  _(NoShapeInfo)                              \
  _(UnknownProperties)                        \
  _(Singleton)                                \
  _(NotSingleton)                             \
  _(NotFixedSlot)                             \
  _(InconsistentFixedSlot)                    \
  _(NotObject)                                \
  _(NotStruct)                                \
  _(NotUndefined)                             \
  _(StructNoField)                            \
  _(NeedsTypeBarrier)                         \
  _(InDictionaryMode)                         \
  _(MultiProtoPaths)                          \
  _(NonWritableProperty)                      \
  _(ProtoIndexedProps)                        \
  _(ArrayBadFlags)                            \
  _(ArrayDoubleConversion)                    \
  _(ArrayRange)                               \
  _(ArraySeenNegativeIndex)                   \
  _(ArraySeenNonIntegerIndex)                 \
  _(TypedObjectHasDetachedBuffer)             \
  _(TypedObjectArrayRange)                    \
  _(AccessNotDense)                           \
  _(AccessNotTypedObject)                     \
  _(AccessNotTypedArray)                      \
  _(AccessNotString)                          \
  _(OperandNotString)                         \
  _(OperandNotNumber)                         \
  _(OperandNotSimpleArith)                    \
  _(OperandNotEasilyCoercibleToString)        \
  _(OutOfBounds)                              \
  _(IndexType)                                \
  _(NotModuleNamespace)                       \
  _(UnknownProperty)                          \
  _(NoTemplateObject)                         \
  _(LengthTooBig)                             \
  _(SpeculationOnInputTypesFailed)            \
  _(RelationalCompare)                        \
  _(OperandTypeNotBitwiseComparable)          \
  _(OperandMaybeEmulatesUndefined)            \
  _(LoosyUndefinedNullCompare)                \
  _(LoosyInt32BooleanCompare)                 \
  _(CallsValueOf)                             \
  _(StrictCompare)                            \
  _(InitHole)                                 \
                                              \
  _(CantInlineGeneric)                        \
  _(CantInlineNoTarget)                       \
  _(CantInlineNotInterpreted)                 \
  _(CantInlineNoBaseline)                     \
  _(CantInlineNoJitScript)                    \
  _(CantInlineLazy)                           \
  _(CantInlineNotConstructor)                 \
  _(CantInlineClassConstructor)               \
  _(CantInlineDisabledIon)                    \
  _(CantInlineTooManyArgs)                    \
  _(CantInlineNeedsArgsObj)                   \
  _(CantInlineDebuggee)                       \
  _(CantInlineExceededDepth)                  \
  _(CantInlineExceededTotalBytecodeLength)    \
  _(CantInlineBigCaller)                      \
  _(CantInlineBigCallee)                      \
  _(CantInlineBigCalleeInlinedBytecodeLength) \
  _(CantInlineCrossRealm)                     \
  _(CantInlineNotHot)                         \
  _(CantInlineNotInDispatch)                  \
  _(CantInlineUnreachable)                    \
  _(CantInlineNativeBadForm)                  \
  _(CantInlineNativeBadType)                  \
  _(CantInlineNativeNoTemplateObj)            \
  _(CantInlineBound)                          \
  _(CantInlineNativeNoSpecialization)         \
  _(CantInlineUnexpectedNewTarget)            \
  _(HasCommonInliningPath)                    \
                                              \
  _(GenericSuccess)                           \
  _(Inlined)                                  \
  _(DOM)                                      \
  _(Monomorphic)                              \
  _(Polymorphic)

#define TRACKED_TYPESITE_LIST(_) \
  _(Receiver)                    \
  _(Operand)                     \
  _(Index)                       \
  _(Value)                       \
  _(Call_Target)                 \
  _(Call_This)                   \
  _(Call_Arg)                    \
  _(Call_Return)

enum class TrackedStrategy : uint32_t {
#define STRATEGY_OP(name) name,
  TRACKED_STRATEGY_LIST(STRATEGY_OP)
#undef STRATEGY_OPT

      Count
};

enum class TrackedOutcome : uint32_t {
#define OUTCOME_OP(name) name,
  TRACKED_OUTCOME_LIST(OUTCOME_OP)
#undef OUTCOME_OP

      Count
};

enum class TrackedTypeSite : uint32_t {
#define TYPESITE_OP(name) name,
  TRACKED_TYPESITE_LIST(TYPESITE_OP)
#undef TYPESITE_OP

      Count
};

JS_PUBLIC_API const char* TrackedStrategyString(TrackedStrategy strategy);

JS_PUBLIC_API const char* TrackedOutcomeString(TrackedOutcome outcome);

JS_PUBLIC_API const char* TrackedTypeSiteString(TrackedTypeSite site);

}  // namespace JS

#endif  // js_TrackedOptimizationInfo_h
