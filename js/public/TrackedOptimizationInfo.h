/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_TrackedOptimizationInfo_h
#define js_TrackedOptimizationInfo_h

#include "mozilla/Maybe.h"

namespace JS {

#define TRACKED_STRATEGY_LIST(_)                        \
    _(GetProp_ArgumentsLength)                          \
    _(GetProp_ArgumentsCallee)                          \
    _(GetProp_InferredConstant)                         \
    _(GetProp_Constant)                                 \
    _(GetProp_SimdGetter)                               \
    _(GetProp_TypedObject)                              \
    _(GetProp_DefiniteSlot)                             \
    _(GetProp_Unboxed)                                  \
    _(GetProp_CommonGetter)                             \
    _(GetProp_InlineAccess)                             \
    _(GetProp_Innerize)                                 \
    _(GetProp_InlineCache)                              \
                                                        \
    _(SetProp_CommonSetter)                             \
    _(SetProp_TypedObject)                              \
    _(SetProp_DefiniteSlot)                             \
    _(SetProp_Unboxed)                                  \
    _(SetProp_InlineAccess)                             \
                                                        \
    _(GetElem_TypedObject)                              \
    _(GetElem_Dense)                                    \
    _(GetElem_TypedStatic)                              \
    _(GetElem_TypedArray)                               \
    _(GetElem_String)                                   \
    _(GetElem_Arguments)                                \
    _(GetElem_ArgumentsInlined)                         \
    _(GetElem_InlineCache)                              \
                                                        \
    _(SetElem_TypedObject)                              \
    _(SetElem_TypedStatic)                              \
    _(SetElem_TypedArray)                               \
    _(SetElem_Dense)                                    \
    _(SetElem_Arguments)                                \
    _(SetElem_InlineCache)                              \
                                                        \
    _(Call_Inline)


// Ordering is important below. All outcomes before GenericSuccess will be
// considered failures, and all outcomes after GenericSuccess will be
// considered successes.
#define TRACKED_OUTCOME_LIST(_)                                         \
    _(GenericFailure)                                                   \
    _(Disabled)                                                         \
    _(NoTypeInfo)                                                       \
    _(NoAnalysisInfo)                                                   \
    _(NoShapeInfo)                                                      \
    _(UnknownObject)                                                    \
    _(UnknownProperties)                                                \
    _(Singleton)                                                        \
    _(NotSingleton)                                                     \
    _(NotFixedSlot)                                                     \
    _(InconsistentFixedSlot)                                            \
    _(NotObject)                                                        \
    _(NotStruct)                                                        \
    _(NotUnboxed)                                                       \
    _(UnboxedConvertedToNative)                                         \
    _(StructNoField)                                                    \
    _(InconsistentFieldType)                                            \
    _(InconsistentFieldOffset)                                          \
    _(NeedsTypeBarrier)                                                 \
    _(InDictionaryMode)                                                 \
    _(NoProtoFound)                                                     \
    _(MultiProtoPaths)                                                  \
    _(NonWritableProperty)                                              \
    _(ProtoIndexedProps)                                                \
    _(ArrayBadFlags)                                                    \
    _(ArrayDoubleConversion)                                            \
    _(ArrayRange)                                                       \
    _(ArraySeenNegativeIndex)                                           \
    _(TypedObjectNeutered)                                              \
    _(TypedObjectArrayRange)                                            \
    _(AccessNotDense)                                                   \
    _(AccessNotSimdObject)                                              \
    _(AccessNotTypedObject)                                             \
    _(AccessNotTypedArray)                                              \
    _(AccessNotString)                                                  \
    _(StaticTypedArrayUint32)                                           \
    _(StaticTypedArrayCantComputeMask)                                  \
    _(OutOfBounds)                                                      \
    _(GetElemStringNotCached)                                           \
    _(NonNativeReceiver)                                                \
    _(IndexType)                                                        \
    _(SetElemNonDenseNonTANotCached)                                    \
    _(NoSimdJitSupport)                                                 \
    _(SimdTypeNotOptimized)                                             \
    _(UnknownSimdProperty)                                              \
                                                                        \
    _(CantInlineGeneric)                                                \
    _(CantInlineNoTarget)                                               \
    _(CantInlineNotInterpreted)                                         \
    _(CantInlineNoBaseline)                                             \
    _(CantInlineLazy)                                                   \
    _(CantInlineNotConstructor)                                         \
    _(CantInlineDisabledIon)                                            \
    _(CantInlineTooManyArgs)                                            \
    _(CantInlineRecursive)                                              \
    _(CantInlineHeavyweight)                                            \
    _(CantInlineNeedsArgsObj)                                           \
    _(CantInlineDebuggee)                                               \
    _(CantInlineUnknownProps)                                           \
    _(CantInlineExceededDepth)                                          \
    _(CantInlineExceededTotalBytecodeLength)                            \
    _(CantInlineBigCaller)                                              \
    _(CantInlineBigCallee)                                              \
    _(CantInlineBigCalleeInlinedBytecodeLength)                         \
    _(CantInlineNotHot)                                                 \
    _(CantInlineNotInDispatch)                                          \
    _(CantInlineUnreachable)                                            \
    _(CantInlineNativeBadForm)                                          \
    _(CantInlineNativeBadType)                                          \
    _(CantInlineNativeNoTemplateObj)                                    \
    _(CantInlineBound)                                                  \
    _(CantInlineNativeNoSpecialization)                                 \
                                                                        \
    _(GenericSuccess)                                                   \
    _(Inlined)                                                          \
    _(DOM)                                                              \
    _(Monomorphic)                                                      \
    _(Polymorphic)

#define TRACKED_TYPESITE_LIST(_)                \
    _(Receiver)                                 \
    _(Index)                                    \
    _(Value)                                    \
    _(Call_Target)                              \
    _(Call_This)                                \
    _(Call_Arg)                                 \
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

JS_PUBLIC_API(const char*)
TrackedStrategyString(TrackedStrategy strategy);

JS_PUBLIC_API(const char*)
TrackedOutcomeString(TrackedOutcome outcome);

JS_PUBLIC_API(const char*)
TrackedTypeSiteString(TrackedTypeSite site);

struct ForEachTrackedOptimizationAttemptOp
{
    virtual void operator()(TrackedStrategy strategy, TrackedOutcome outcome) = 0;
};

JS_PUBLIC_API(void)
ForEachTrackedOptimizationAttempt(JSRuntime* rt, void* addr, uint8_t index,
                                  ForEachTrackedOptimizationAttemptOp& op,
                                  JSScript** scriptOut, jsbytecode** pcOut);

struct ForEachTrackedOptimizationTypeInfoOp
{
    // Called 0+ times per entry, once for each type in the type set that Ion
    // saw during MIR construction. readType is always called _before_
    // operator() on the same entry.
    //
    // The keyedBy parameter describes how the type is keyed:
    //   - "primitive"   for primitive types
    //   - "constructor" for object types tied to a scripted constructor
    //                   function.
    //   - "alloc site"  for object types tied to an allocation site.
    //   - "prototype"   for object types tied neither to a constructor nor
    //                   to an allocation site, but to a prototype.
    //   - "singleton"   for object types which only has a single value.
    //   - "function"    for object types referring to scripted functions.
    //   - "native"      for object types referring to native functions.
    //
    // The name parameter is the string representation of the type. If the
    // type is keyed by "constructor", or if the type itself refers to a
    // scripted function, the name is the function's displayAtom. If the type
    // is keyed by "native", this is nullptr.
    //
    // The location parameter is the filename if the type is keyed by
    // "constructor", "alloc site", or if the type itself refers to a scripted
    // function. If the type is keyed by "native", it is the offset of the
    // native function, suitable for use with addr2line on Linux or atos on OS
    // X. Otherwise it is nullptr.
    //
    // The lineno parameter is the line number if the type is keyed by
    // "constructor", "alloc site", or if the type itself refers to a scripted
    // function. Otherwise it is Nothing().
    //
    // The location parameter is the only one that may need escaping if being
    // quoted.
    virtual void readType(const char* keyedBy, const char* name,
                          const char* location, mozilla::Maybe<unsigned> lineno) = 0;

    // Called once per entry.
    virtual void operator()(TrackedTypeSite site, const char* mirType) = 0;
};

JS_PUBLIC_API(void)
ForEachTrackedOptimizationTypeInfo(JSRuntime* rt, void* addr, uint8_t index,
                                   ForEachTrackedOptimizationTypeInfoOp& op);

JS_PUBLIC_API(mozilla::Maybe<uint8_t>)
TrackedOptimizationIndexAtAddr(JSRuntime* rt, void* addr, void** entryAddr);

} // namespace JS

#endif // js_TrackedOptimizationInfo_h
