/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Casting.h"

#include "jsmath.h"

#include "builtin/AtomicsObject.h"
#ifdef JS_HAS_INTL_API
#  include "builtin/intl/Collator.h"
#  include "builtin/intl/DateTimeFormat.h"
#  include "builtin/intl/ListFormat.h"
#  include "builtin/intl/NumberFormat.h"
#  include "builtin/intl/PluralRules.h"
#  include "builtin/intl/RelativeTimeFormat.h"
#endif
#include "builtin/MapObject.h"
#include "builtin/String.h"
#include "builtin/TestingFunctions.h"
#include "builtin/TypedObject.h"
#include "jit/BaselineInspector.h"
#include "jit/InlinableNatives.h"
#include "jit/IonBuilder.h"
#include "jit/Lowering.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"
#include "js/RegExpFlags.h"  // JS::RegExpFlag, JS::RegExpFlags
#include "vm/ArgumentsObject.h"
#include "vm/ArrayBufferObject.h"
#include "vm/JSObject.h"
#include "vm/PlainObject.h"  // js::PlainObject
#include "vm/ProxyObject.h"
#include "vm/SelfHosting.h"
#include "vm/SharedArrayObject.h"
#include "vm/TypedArrayObject.h"
#include "wasm/WasmInstance.h"

#include "jit/shared/Lowering-shared-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/StringObject-inl.h"

using mozilla::ArrayLength;
using mozilla::AssertedCast;
using mozilla::Maybe;

using JS::RegExpFlag;
using JS::RegExpFlags;

namespace js {
namespace jit {

// Returns true if |native| can be inlined cross-realm. Especially inlined
// natives that can allocate objects or throw exceptions shouldn't be inlined
// cross-realm without a careful analysis because we might use the wrong realm!
//
// Note that self-hosting intrinsics are never called cross-realm. See the
// MOZ_CRASH below.
//
// If you are adding a new inlinable native, the safe thing is to |return false|
// here.
static bool CanInlineCrossRealm(InlinableNative native) {
  switch (native) {
    case InlinableNative::MathAbs:
    case InlinableNative::MathFloor:
    case InlinableNative::MathCeil:
    case InlinableNative::MathRound:
    case InlinableNative::MathClz32:
    case InlinableNative::MathSqrt:
    case InlinableNative::MathATan2:
    case InlinableNative::MathHypot:
    case InlinableNative::MathMax:
    case InlinableNative::MathMin:
    case InlinableNative::MathPow:
    case InlinableNative::MathImul:
    case InlinableNative::MathFRound:
    case InlinableNative::MathTrunc:
    case InlinableNative::MathSign:
    case InlinableNative::MathSin:
    case InlinableNative::MathTan:
    case InlinableNative::MathCos:
    case InlinableNative::MathExp:
    case InlinableNative::MathLog:
    case InlinableNative::MathASin:
    case InlinableNative::MathATan:
    case InlinableNative::MathACos:
    case InlinableNative::MathLog10:
    case InlinableNative::MathLog2:
    case InlinableNative::MathLog1P:
    case InlinableNative::MathExpM1:
    case InlinableNative::MathCosH:
    case InlinableNative::MathSinH:
    case InlinableNative::MathTanH:
    case InlinableNative::MathACosH:
    case InlinableNative::MathASinH:
    case InlinableNative::MathATanH:
    case InlinableNative::MathCbrt:
    case InlinableNative::Boolean:
      return true;

    case InlinableNative::Array:
      // Cross-realm case handled by inlineArray.
      return true;

    case InlinableNative::MathRandom:
      // RNG state is per-realm.
      return false;

    case InlinableNative::IntlGuardToCollator:
    case InlinableNative::IntlGuardToDateTimeFormat:
    case InlinableNative::IntlGuardToListFormat:
    case InlinableNative::IntlGuardToNumberFormat:
    case InlinableNative::IntlGuardToPluralRules:
    case InlinableNative::IntlGuardToRelativeTimeFormat:
    case InlinableNative::IsRegExpObject:
    case InlinableNative::IsPossiblyWrappedRegExpObject:
    case InlinableNative::RegExpMatcher:
    case InlinableNative::RegExpSearcher:
    case InlinableNative::RegExpTester:
    case InlinableNative::RegExpPrototypeOptimizable:
    case InlinableNative::RegExpInstanceOptimizable:
    case InlinableNative::GetFirstDollarIndex:
    case InlinableNative::IntrinsicNewArrayIterator:
    case InlinableNative::IntrinsicNewStringIterator:
    case InlinableNative::IntrinsicNewRegExpStringIterator:
    case InlinableNative::IntrinsicStringReplaceString:
    case InlinableNative::IntrinsicStringSplitString:
    case InlinableNative::IntrinsicUnsafeSetReservedSlot:
    case InlinableNative::IntrinsicUnsafeGetReservedSlot:
    case InlinableNative::IntrinsicUnsafeGetObjectFromReservedSlot:
    case InlinableNative::IntrinsicUnsafeGetInt32FromReservedSlot:
    case InlinableNative::IntrinsicUnsafeGetStringFromReservedSlot:
    case InlinableNative::IntrinsicUnsafeGetBooleanFromReservedSlot:
    case InlinableNative::IntrinsicIsCallable:
    case InlinableNative::IntrinsicIsConstructor:
    case InlinableNative::IntrinsicToObject:
    case InlinableNative::IntrinsicIsObject:
    case InlinableNative::IntrinsicIsCrossRealmArrayConstructor:
    case InlinableNative::IntrinsicToInteger:
    case InlinableNative::IntrinsicToString:
    case InlinableNative::IntrinsicIsConstructing:
    case InlinableNative::IntrinsicSubstringKernel:
    case InlinableNative::IntrinsicGuardToArrayIterator:
    case InlinableNative::IntrinsicGuardToMapIterator:
    case InlinableNative::IntrinsicGuardToSetIterator:
    case InlinableNative::IntrinsicGuardToStringIterator:
    case InlinableNative::IntrinsicGuardToRegExpStringIterator:
    case InlinableNative::IntrinsicObjectHasPrototype:
    case InlinableNative::IntrinsicFinishBoundFunctionInit:
    case InlinableNative::IntrinsicIsPackedArray:
    case InlinableNative::IntrinsicGuardToMapObject:
    case InlinableNative::IntrinsicGetNextMapEntryForIterator:
    case InlinableNative::IntrinsicGuardToSetObject:
    case InlinableNative::IntrinsicGetNextSetEntryForIterator:
    case InlinableNative::IntrinsicGuardToArrayBuffer:
    case InlinableNative::IntrinsicArrayBufferByteLength:
    case InlinableNative::IntrinsicPossiblyWrappedArrayBufferByteLength:
    case InlinableNative::IntrinsicGuardToSharedArrayBuffer:
    case InlinableNative::IntrinsicIsTypedArrayConstructor:
    case InlinableNative::IntrinsicIsTypedArray:
    case InlinableNative::IntrinsicIsPossiblyWrappedTypedArray:
    case InlinableNative::IntrinsicPossiblyWrappedTypedArrayLength:
    case InlinableNative::IntrinsicTypedArrayLength:
    case InlinableNative::IntrinsicTypedArrayByteOffset:
    case InlinableNative::IntrinsicTypedArrayElementShift:
    case InlinableNative::IntrinsicArrayIteratorPrototypeOptimizable:
      MOZ_CRASH("Unexpected cross-realm intrinsic call");

    case InlinableNative::TestBailout:
    case InlinableNative::TestAssertFloat32:
    case InlinableNative::TestAssertRecoveredOnBailout:
      // Testing functions, not worth inlining cross-realm.
      return false;

    case InlinableNative::ArrayIsArray:
    case InlinableNative::ArrayJoin:
    case InlinableNative::ArrayPop:
    case InlinableNative::ArrayShift:
    case InlinableNative::ArrayPush:
    case InlinableNative::ArraySlice:
    case InlinableNative::AtomicsCompareExchange:
    case InlinableNative::AtomicsExchange:
    case InlinableNative::AtomicsLoad:
    case InlinableNative::AtomicsStore:
    case InlinableNative::AtomicsAdd:
    case InlinableNative::AtomicsSub:
    case InlinableNative::AtomicsAnd:
    case InlinableNative::AtomicsOr:
    case InlinableNative::AtomicsXor:
    case InlinableNative::AtomicsIsLockFree:
    case InlinableNative::ReflectGetPrototypeOf:
    case InlinableNative::String:
    case InlinableNative::StringCharCodeAt:
    case InlinableNative::StringFromCharCode:
    case InlinableNative::StringFromCodePoint:
    case InlinableNative::StringCharAt:
    case InlinableNative::StringToLowerCase:
    case InlinableNative::StringToUpperCase:
    case InlinableNative::Object:
    case InlinableNative::ObjectCreate:
    case InlinableNative::ObjectIs:
    case InlinableNative::ObjectToString:
    case InlinableNative::TypedArrayConstructor:
      // Default to false for most natives.
      return false;

    case InlinableNative::Limit:
      break;
  }
  MOZ_CRASH("Unknown native");
}

IonBuilder::InliningResult IonBuilder::inlineNativeCall(CallInfo& callInfo,
                                                        JSFunction* target) {
  MOZ_ASSERT(target->isNative());

  if (!optimizationInfo().inlineNative()) {
    return InliningStatus_NotInlined;
  }

  bool isWasmCall = target->isWasmWithJitEntry();
  if (!isWasmCall &&
      (!target->hasJitInfo() ||
       target->jitInfo()->type() != JSJitInfo::InlinableNative)) {
    // Reaching here means we tried to inline a native for which there is no
    // Ion specialization.
    return InliningStatus_NotInlined;
  }

  // Don't inline if we're constructing and new.target != callee. This can
  // happen with Reflect.construct or derived class constructors.
  if (callInfo.constructing() && callInfo.getNewTarget() != callInfo.fun()) {
    return InliningStatus_NotInlined;
  }

  if (shouldAbortOnPreliminaryGroups(callInfo.thisArg())) {
    return InliningStatus_NotInlined;
  }
  for (size_t i = 0; i < callInfo.argc(); i++) {
    if (shouldAbortOnPreliminaryGroups(callInfo.getArg(i))) {
      return InliningStatus_NotInlined;
    }
  }

  if (isWasmCall) {
    return inlineWasmCall(callInfo, target);
  }

  InlinableNative inlNative = target->jitInfo()->inlinableNative;

  if (target->realm() != script()->realm() && !CanInlineCrossRealm(inlNative)) {
    return InliningStatus_NotInlined;
  }

  switch (inlNative) {
    // Array natives.
    case InlinableNative::Array:
      return inlineArray(callInfo, target->realm());
    case InlinableNative::ArrayIsArray:
      return inlineArrayIsArray(callInfo);
    case InlinableNative::ArrayJoin:
      return inlineArrayJoin(callInfo);
    case InlinableNative::ArrayPop:
      return inlineArrayPopShift(callInfo, MArrayPopShift::Pop);
    case InlinableNative::ArrayShift:
      return inlineArrayPopShift(callInfo, MArrayPopShift::Shift);
    case InlinableNative::ArrayPush:
      return inlineArrayPush(callInfo);
    case InlinableNative::ArraySlice:
      return inlineArraySlice(callInfo);

    // Array intrinsics.
    case InlinableNative::IntrinsicNewArrayIterator:
      return inlineNewIterator(callInfo, MNewIterator::ArrayIterator);
    case InlinableNative::IntrinsicArrayIteratorPrototypeOptimizable:
      return inlineArrayIteratorPrototypeOptimizable(callInfo);

    // Atomic natives.
    case InlinableNative::AtomicsCompareExchange:
      return inlineAtomicsCompareExchange(callInfo);
    case InlinableNative::AtomicsExchange:
      return inlineAtomicsExchange(callInfo);
    case InlinableNative::AtomicsLoad:
      return inlineAtomicsLoad(callInfo);
    case InlinableNative::AtomicsStore:
      return inlineAtomicsStore(callInfo);
    case InlinableNative::AtomicsAdd:
    case InlinableNative::AtomicsSub:
    case InlinableNative::AtomicsAnd:
    case InlinableNative::AtomicsOr:
    case InlinableNative::AtomicsXor:
      return inlineAtomicsBinop(callInfo, inlNative);
    case InlinableNative::AtomicsIsLockFree:
      return inlineAtomicsIsLockFree(callInfo);

    // Boolean natives.
    case InlinableNative::Boolean:
      return inlineBoolean(callInfo);

#ifdef JS_HAS_INTL_API
    // Intl natives.
    case InlinableNative::IntlGuardToCollator:
      return inlineGuardToClass(callInfo, &CollatorObject::class_);
    case InlinableNative::IntlGuardToDateTimeFormat:
      return inlineGuardToClass(callInfo, &DateTimeFormatObject::class_);
    case InlinableNative::IntlGuardToListFormat:
      return inlineGuardToClass(callInfo, &ListFormatObject::class_);
    case InlinableNative::IntlGuardToNumberFormat:
      return inlineGuardToClass(callInfo, &NumberFormatObject::class_);
    case InlinableNative::IntlGuardToPluralRules:
      return inlineGuardToClass(callInfo, &PluralRulesObject::class_);
    case InlinableNative::IntlGuardToRelativeTimeFormat:
      return inlineGuardToClass(callInfo, &RelativeTimeFormatObject::class_);
#else
    case InlinableNative::IntlGuardToCollator:
    case InlinableNative::IntlGuardToDateTimeFormat:
    case InlinableNative::IntlGuardToListFormat:
    case InlinableNative::IntlGuardToNumberFormat:
    case InlinableNative::IntlGuardToPluralRules:
    case InlinableNative::IntlGuardToRelativeTimeFormat:
      MOZ_CRASH("Intl API disabled");
#endif

    // Math natives.
    case InlinableNative::MathAbs:
      return inlineMathAbs(callInfo);
    case InlinableNative::MathFloor:
      return inlineMathFloor(callInfo);
    case InlinableNative::MathCeil:
      return inlineMathCeil(callInfo);
    case InlinableNative::MathRound:
      return inlineMathRound(callInfo);
    case InlinableNative::MathClz32:
      return inlineMathClz32(callInfo);
    case InlinableNative::MathSqrt:
      return inlineMathSqrt(callInfo);
    case InlinableNative::MathATan2:
      return inlineMathAtan2(callInfo);
    case InlinableNative::MathHypot:
      return inlineMathHypot(callInfo);
    case InlinableNative::MathMax:
      return inlineMathMinMax(callInfo, true /* max */);
    case InlinableNative::MathMin:
      return inlineMathMinMax(callInfo, false /* max */);
    case InlinableNative::MathPow:
      return inlineMathPow(callInfo);
    case InlinableNative::MathRandom:
      return inlineMathRandom(callInfo);
    case InlinableNative::MathImul:
      return inlineMathImul(callInfo);
    case InlinableNative::MathFRound:
      return inlineMathFRound(callInfo);
    case InlinableNative::MathTrunc:
      return inlineMathTrunc(callInfo);
    case InlinableNative::MathSign:
      return inlineMathSign(callInfo);
    case InlinableNative::MathSin:
      return inlineMathFunction(callInfo, MMathFunction::Sin);
    case InlinableNative::MathTan:
      return inlineMathFunction(callInfo, MMathFunction::Tan);
    case InlinableNative::MathCos:
      return inlineMathFunction(callInfo, MMathFunction::Cos);
    case InlinableNative::MathExp:
      return inlineMathFunction(callInfo, MMathFunction::Exp);
    case InlinableNative::MathLog:
      return inlineMathFunction(callInfo, MMathFunction::Log);
    case InlinableNative::MathASin:
      return inlineMathFunction(callInfo, MMathFunction::ASin);
    case InlinableNative::MathATan:
      return inlineMathFunction(callInfo, MMathFunction::ATan);
    case InlinableNative::MathACos:
      return inlineMathFunction(callInfo, MMathFunction::ACos);
    case InlinableNative::MathLog10:
      return inlineMathFunction(callInfo, MMathFunction::Log10);
    case InlinableNative::MathLog2:
      return inlineMathFunction(callInfo, MMathFunction::Log2);
    case InlinableNative::MathLog1P:
      return inlineMathFunction(callInfo, MMathFunction::Log1P);
    case InlinableNative::MathExpM1:
      return inlineMathFunction(callInfo, MMathFunction::ExpM1);
    case InlinableNative::MathCosH:
      return inlineMathFunction(callInfo, MMathFunction::CosH);
    case InlinableNative::MathSinH:
      return inlineMathFunction(callInfo, MMathFunction::SinH);
    case InlinableNative::MathTanH:
      return inlineMathFunction(callInfo, MMathFunction::TanH);
    case InlinableNative::MathACosH:
      return inlineMathFunction(callInfo, MMathFunction::ACosH);
    case InlinableNative::MathASinH:
      return inlineMathFunction(callInfo, MMathFunction::ASinH);
    case InlinableNative::MathATanH:
      return inlineMathFunction(callInfo, MMathFunction::ATanH);
    case InlinableNative::MathCbrt:
      return inlineMathFunction(callInfo, MMathFunction::Cbrt);

    // Reflect natives.
    case InlinableNative::ReflectGetPrototypeOf:
      return inlineReflectGetPrototypeOf(callInfo);

    // RegExp natives.
    case InlinableNative::RegExpMatcher:
      return inlineRegExpMatcher(callInfo);
    case InlinableNative::RegExpSearcher:
      return inlineRegExpSearcher(callInfo);
    case InlinableNative::RegExpTester:
      return inlineRegExpTester(callInfo);
    case InlinableNative::IsRegExpObject:
      return inlineIsRegExpObject(callInfo);
    case InlinableNative::IsPossiblyWrappedRegExpObject:
      return inlineIsPossiblyWrappedRegExpObject(callInfo);
    case InlinableNative::RegExpPrototypeOptimizable:
      return inlineRegExpPrototypeOptimizable(callInfo);
    case InlinableNative::RegExpInstanceOptimizable:
      return inlineRegExpInstanceOptimizable(callInfo);
    case InlinableNative::GetFirstDollarIndex:
      return inlineGetFirstDollarIndex(callInfo);
    case InlinableNative::IntrinsicNewRegExpStringIterator:
      return inlineNewIterator(callInfo, MNewIterator::RegExpStringIterator);

    // String natives.
    case InlinableNative::String:
      return inlineStringObject(callInfo);
    case InlinableNative::StringCharCodeAt:
      return inlineStrCharCodeAt(callInfo);
    case InlinableNative::StringFromCharCode:
      return inlineStrFromCharCode(callInfo);
    case InlinableNative::StringFromCodePoint:
      return inlineStrFromCodePoint(callInfo);
    case InlinableNative::StringCharAt:
      return inlineStrCharAt(callInfo);
    case InlinableNative::StringToLowerCase:
      return inlineStringConvertCase(callInfo, MStringConvertCase::LowerCase);
    case InlinableNative::StringToUpperCase:
      return inlineStringConvertCase(callInfo, MStringConvertCase::UpperCase);

    // String intrinsics.
    case InlinableNative::IntrinsicStringReplaceString:
      return inlineStringReplaceString(callInfo);
    case InlinableNative::IntrinsicStringSplitString:
      return inlineStringSplitString(callInfo);
    case InlinableNative::IntrinsicNewStringIterator:
      return inlineNewIterator(callInfo, MNewIterator::StringIterator);

    // Object natives.
    case InlinableNative::Object:
      return inlineObject(callInfo);
    case InlinableNative::ObjectCreate:
      return inlineObjectCreate(callInfo);
    case InlinableNative::ObjectIs:
      return inlineObjectIs(callInfo);
    case InlinableNative::ObjectToString:
      return inlineObjectToString(callInfo);

    // Testing functions.
    case InlinableNative::TestBailout:
      return inlineBailout(callInfo);
    case InlinableNative::TestAssertFloat32:
      return inlineAssertFloat32(callInfo);
    case InlinableNative::TestAssertRecoveredOnBailout:
      return inlineAssertRecoveredOnBailout(callInfo);

    // Slot intrinsics.
    case InlinableNative::IntrinsicUnsafeSetReservedSlot:
      return inlineUnsafeSetReservedSlot(callInfo);
    case InlinableNative::IntrinsicUnsafeGetReservedSlot:
      return inlineUnsafeGetReservedSlot(callInfo, MIRType::Value);
    case InlinableNative::IntrinsicUnsafeGetObjectFromReservedSlot:
      return inlineUnsafeGetReservedSlot(callInfo, MIRType::Object);
    case InlinableNative::IntrinsicUnsafeGetInt32FromReservedSlot:
      return inlineUnsafeGetReservedSlot(callInfo, MIRType::Int32);
    case InlinableNative::IntrinsicUnsafeGetStringFromReservedSlot:
      return inlineUnsafeGetReservedSlot(callInfo, MIRType::String);
    case InlinableNative::IntrinsicUnsafeGetBooleanFromReservedSlot:
      return inlineUnsafeGetReservedSlot(callInfo, MIRType::Boolean);

    // Utility intrinsics.
    case InlinableNative::IntrinsicIsCallable:
      return inlineIsCallable(callInfo);
    case InlinableNative::IntrinsicIsConstructor:
      return inlineIsConstructor(callInfo);
    case InlinableNative::IntrinsicToObject:
      return inlineToObject(callInfo);
    case InlinableNative::IntrinsicIsObject:
      return inlineIsObject(callInfo);
    case InlinableNative::IntrinsicIsCrossRealmArrayConstructor:
      return inlineIsCrossRealmArrayConstructor(callInfo);
    case InlinableNative::IntrinsicToInteger:
      return inlineToInteger(callInfo);
    case InlinableNative::IntrinsicToString:
      return inlineToString(callInfo);
    case InlinableNative::IntrinsicIsConstructing:
      return inlineIsConstructing(callInfo);
    case InlinableNative::IntrinsicSubstringKernel:
      return inlineSubstringKernel(callInfo);
    case InlinableNative::IntrinsicGuardToArrayIterator:
      return inlineGuardToClass(callInfo, &ArrayIteratorObject::class_);
    case InlinableNative::IntrinsicGuardToMapIterator:
      return inlineGuardToClass(callInfo, &MapIteratorObject::class_);
    case InlinableNative::IntrinsicGuardToSetIterator:
      return inlineGuardToClass(callInfo, &SetIteratorObject::class_);
    case InlinableNative::IntrinsicGuardToStringIterator:
      return inlineGuardToClass(callInfo, &StringIteratorObject::class_);
    case InlinableNative::IntrinsicGuardToRegExpStringIterator:
      return inlineGuardToClass(callInfo, &RegExpStringIteratorObject::class_);
    case InlinableNative::IntrinsicObjectHasPrototype:
      return inlineObjectHasPrototype(callInfo);
    case InlinableNative::IntrinsicFinishBoundFunctionInit:
      return inlineFinishBoundFunctionInit(callInfo);
    case InlinableNative::IntrinsicIsPackedArray:
      return inlineIsPackedArray(callInfo);

    // Map intrinsics.
    case InlinableNative::IntrinsicGuardToMapObject:
      return inlineGuardToClass(callInfo, &MapObject::class_);
    case InlinableNative::IntrinsicGetNextMapEntryForIterator:
      return inlineGetNextEntryForIterator(callInfo,
                                           MGetNextEntryForIterator::Map);

    // Set intrinsics.
    case InlinableNative::IntrinsicGuardToSetObject:
      return inlineGuardToClass(callInfo, &SetObject::class_);
    case InlinableNative::IntrinsicGetNextSetEntryForIterator:
      return inlineGetNextEntryForIterator(callInfo,
                                           MGetNextEntryForIterator::Set);

    // ArrayBuffer intrinsics.
    case InlinableNative::IntrinsicGuardToArrayBuffer:
      return inlineGuardToClass(callInfo, &ArrayBufferObject::class_);
    case InlinableNative::IntrinsicArrayBufferByteLength:
      return inlineArrayBufferByteLength(callInfo);
    case InlinableNative::IntrinsicPossiblyWrappedArrayBufferByteLength:
      return inlinePossiblyWrappedArrayBufferByteLength(callInfo);

    // SharedArrayBuffer intrinsics.
    case InlinableNative::IntrinsicGuardToSharedArrayBuffer:
      return inlineGuardToClass(callInfo, &SharedArrayBufferObject::class_);

    // TypedArray intrinsics.
    case InlinableNative::TypedArrayConstructor:
      return inlineTypedArray(callInfo, target->native());
    case InlinableNative::IntrinsicIsTypedArrayConstructor:
      return inlineIsTypedArrayConstructor(callInfo);
    case InlinableNative::IntrinsicIsTypedArray:
      return inlineIsTypedArray(callInfo);
    case InlinableNative::IntrinsicIsPossiblyWrappedTypedArray:
      return inlineIsPossiblyWrappedTypedArray(callInfo);
    case InlinableNative::IntrinsicPossiblyWrappedTypedArrayLength:
      return inlinePossiblyWrappedTypedArrayLength(callInfo);
    case InlinableNative::IntrinsicTypedArrayLength:
      return inlineTypedArrayLength(callInfo);
    case InlinableNative::IntrinsicTypedArrayByteOffset:
      return inlineTypedArrayByteOffset(callInfo);
    case InlinableNative::IntrinsicTypedArrayElementShift:
      return inlineTypedArrayElementShift(callInfo);

    case InlinableNative::Limit:
      break;
  }

  MOZ_CRASH("Shouldn't get here");
}

IonBuilder::InliningResult IonBuilder::inlineNativeGetter(CallInfo& callInfo,
                                                          JSFunction* target) {
  MOZ_ASSERT(target->isNative());
  JSNative native = target->native();

  if (!optimizationInfo().inlineNative()) {
    return InliningStatus_NotInlined;
  }

  MDefinition* thisArg = callInfo.thisArg();
  TemporaryTypeSet* thisTypes = thisArg->resultTypeSet();
  MOZ_ASSERT(callInfo.argc() == 0);

  if (!thisTypes) {
    return InliningStatus_NotInlined;
  }

  // Note: target might be a cross-realm native!

  // Try to optimize typed array lengths.
  if (TypedArrayObject::isOriginalLengthGetter(native)) {
    if (thisTypes->forAllClasses(constraints(), IsTypedArrayClass) !=
        TemporaryTypeSet::ForAllResult::ALL_TRUE) {
      return InliningStatus_NotInlined;
    }

    MInstruction* length = addTypedArrayLength(thisArg);
    current->push(length);
    return InliningStatus_Inlined;
  }

  // Try to optimize typed array byteOffsets.
  if (TypedArrayObject::isOriginalByteOffsetGetter(native)) {
    if (thisTypes->forAllClasses(constraints(), IsTypedArrayClass) !=
        TemporaryTypeSet::ForAllResult::ALL_TRUE) {
      return InliningStatus_NotInlined;
    }

    MInstruction* byteOffset = addTypedArrayByteOffset(thisArg);
    current->push(byteOffset);
    return InliningStatus_Inlined;
  }

  // Try to optimize RegExp getters.
  RegExpFlags mask = RegExpFlag::NoFlags;
  if (RegExpObject::isOriginalFlagGetter(native, &mask)) {
    const JSClass* clasp = thisTypes->getKnownClass(constraints());
    if (clasp != &RegExpObject::class_) {
      return InliningStatus_NotInlined;
    }

    MLoadFixedSlot* flags =
        MLoadFixedSlot::New(alloc(), thisArg, RegExpObject::flagsSlot());
    current->add(flags);
    flags->setResultType(MIRType::Int32);
    MConstant* maskConst = MConstant::New(alloc(), Int32Value(mask.value()));
    current->add(maskConst);
    auto* maskedFlag = MBitAnd::New(alloc(), flags, maskConst, MIRType::Int32);
    current->add(maskedFlag);

    MDefinition* result = convertToBoolean(maskedFlag);
    current->push(result);
    return InliningStatus_Inlined;
  }

  return InliningStatus_NotInlined;
}

TemporaryTypeSet* IonBuilder::getInlineReturnTypeSet() {
  return bytecodeTypes(pc);
}

MIRType IonBuilder::getInlineReturnType() {
  TemporaryTypeSet* returnTypes = getInlineReturnTypeSet();
  return returnTypes->getKnownMIRType();
}

IonBuilder::InliningResult IonBuilder::inlineMathFunction(
    CallInfo& callInfo, MMathFunction::Function function) {
  if (callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  if (callInfo.argc() != 1) {
    return InliningStatus_NotInlined;
  }

  if (getInlineReturnType() != MIRType::Double) {
    return InliningStatus_NotInlined;
  }
  if (!IsNumberType(callInfo.getArg(0)->type())) {
    return InliningStatus_NotInlined;
  }

  callInfo.fun()->setImplicitlyUsedUnchecked();
  callInfo.thisArg()->setImplicitlyUsedUnchecked();

  MMathFunction* ins =
      MMathFunction::New(alloc(), callInfo.getArg(0), function);
  current->add(ins);
  current->push(ins);
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineArray(CallInfo& callInfo,
                                                   Realm* targetRealm) {
  uint32_t initLength = 0;

  JSObject* templateObject =
      inspector->getTemplateObjectForNative(pc, ArrayConstructor);
  // This is shared by ArrayConstructor and array_construct (std_Array).
  if (!templateObject) {
    templateObject = inspector->getTemplateObjectForNative(pc, array_construct);
  }

  if (!templateObject) {
    return InliningStatus_NotInlined;
  }

  if (templateObject->nonCCWRealm() != targetRealm) {
    return InliningStatus_NotInlined;
  }

  // Multiple arguments imply array initialization, not just construction.
  if (callInfo.argc() >= 2) {
    initLength = callInfo.argc();

    TypeSet::ObjectKey* key = TypeSet::ObjectKey::get(templateObject);
    if (!key->unknownProperties()) {
      HeapTypeSetKey elemTypes = key->property(JSID_VOID);

      for (uint32_t i = 0; i < initLength; i++) {
        MDefinition* value = callInfo.getArg(i);
        if (!TypeSetIncludes(elemTypes.maybeTypes(), value->type(),
                             value->resultTypeSet())) {
          elemTypes.freeze(constraints());
          return InliningStatus_NotInlined;
        }
      }
    }
  }

  // A single integer argument denotes initial length.
  if (callInfo.argc() == 1) {
    MDefinition* arg = callInfo.getArg(0);
    if (arg->type() != MIRType::Int32) {
      return InliningStatus_NotInlined;
    }

    if (!arg->isConstant()) {
      callInfo.setImplicitlyUsedUnchecked();
      MNewArrayDynamicLength* ins = MNewArrayDynamicLength::New(
          alloc(), constraints(), templateObject,
          templateObject->group()->initialHeap(constraints()), arg);
      current->add(ins);
      current->push(ins);

      // This may throw, so we need a resume point.
      MOZ_TRY(resumeAfter(ins));

      return InliningStatus_Inlined;
    }

    // Negative lengths generate a RangeError, unhandled by the inline path.
    initLength = arg->toConstant()->toInt32();
    if (initLength > NativeObject::MAX_DENSE_ELEMENTS_COUNT) {
      return InliningStatus_NotInlined;
    }
    MOZ_ASSERT(initLength <= INT32_MAX);

    // Make sure initLength matches the template object's length. This is
    // not guaranteed to be the case, for instance if we're inlining the
    // MConstant may come from an outer script.
    if (initLength != templateObject->as<ArrayObject>().length()) {
      return InliningStatus_NotInlined;
    }

    // Don't inline large allocations.
    if (initLength > ArrayObject::EagerAllocationMaxLength) {
      return InliningStatus_NotInlined;
    }
  }

  callInfo.setImplicitlyUsedUnchecked();

  MOZ_TRY(jsop_newarray(templateObject, initLength));

  MNewArray* array = current->peek(-1)->toNewArray();
  if (callInfo.argc() >= 2) {
    for (uint32_t i = 0; i < initLength; i++) {
      if (!alloc().ensureBallast()) {
        return abort(AbortReason::Alloc);
      }
      MDefinition* value = callInfo.getArg(i);

      MConstant* id = MConstant::New(alloc(), Int32Value(i));
      current->add(id);

      MOZ_TRY(initArrayElementFastPath(array, id, value,
                                       /* addResumePoint = */ false));
    }

    MInstruction* setLength = setInitializedLength(array, initLength);
    MOZ_TRY(resumeAfter(setLength));
  }

  return InliningStatus_Inlined;
}

static bool IsArrayClass(const JSClass* clasp) {
  return clasp == &ArrayObject::class_;
}

IonBuilder::InliningResult IonBuilder::inlineArrayIsArray(CallInfo& callInfo) {
  if (callInfo.constructing() || callInfo.argc() != 1) {
    return InliningStatus_NotInlined;
  }

  if (getInlineReturnType() != MIRType::Boolean) {
    return InliningStatus_NotInlined;
  }

  MDefinition* arg = callInfo.getArg(0);

  if (!arg->mightBeType(MIRType::Object)) {
    pushConstant(BooleanValue(false));
    callInfo.setImplicitlyUsedUnchecked();
    return InliningStatus_Inlined;
  }

  using ForAllResult = TemporaryTypeSet::ForAllResult;

  TemporaryTypeSet* types = arg->resultTypeSet();

  // Fast path for non-proxy objects.
  if (arg->type() == MIRType::Object && types &&
      types->forAllClasses(constraints(), IsProxyClass) ==
          ForAllResult::ALL_FALSE) {
    // Definitely not a proxy. Now check for the array classes.
    ForAllResult result = types->forAllClasses(constraints(), IsArrayClass);

    if (result == ForAllResult::ALL_FALSE || result == ForAllResult::ALL_TRUE) {
      // Definitely an array or definitely not an array, so we can
      // constant fold.
      pushConstant(BooleanValue(result == ForAllResult::ALL_TRUE));
      callInfo.setImplicitlyUsedUnchecked();
      return InliningStatus_Inlined;
    }

    // We have some array classes and some non-array classes, so we have to
    // check at runtime.
    MOZ_ASSERT(result == ForAllResult::MIXED);

    MHasClass* hasClass = MHasClass::New(alloc(), arg, &ArrayObject::class_);
    current->add(hasClass);
    current->push(hasClass);

    callInfo.setImplicitlyUsedUnchecked();
    return InliningStatus_Inlined;
  }

  // The value might be a primitive or a proxy. MIsArray handles these cases.
  MIsArray* isArray = MIsArray::New(alloc(), arg);
  current->add(isArray);
  current->push(isArray);

  MOZ_TRY(resumeAfter(isArray));

  callInfo.setImplicitlyUsedUnchecked();
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineArrayPopShift(
    CallInfo& callInfo, MArrayPopShift::Mode mode) {
  if (callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  MIRType returnType = getInlineReturnType();
  if (returnType == MIRType::Undefined || returnType == MIRType::Null) {
    return InliningStatus_NotInlined;
  }
  if (callInfo.thisArg()->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }

  // Pop and shift are only handled for dense arrays that have never been
  // used in an iterator: popping elements does not account for suppressing
  // deleted properties in active iterators.
  // Don't optimize shift if the array may be non-extensible (this matters
  // when there are holes). Don't optimize pop if the array may be
  // non-extensible, so we don't need to adjust the capacity for
  // non-extensible arrays (non-extensible objects always have a capacity
  // equal to their initialized length). We check this here because there's
  // no non-extensible ObjectElements flag so we would need an extra guard
  // on the BaseShape flags.
  ObjectGroupFlags unhandledFlags =
      OBJECT_FLAG_SPARSE_INDEXES | OBJECT_FLAG_LENGTH_OVERFLOW |
      OBJECT_FLAG_ITERATED | OBJECT_FLAG_NON_EXTENSIBLE_ELEMENTS;

  MDefinition* obj = callInfo.thisArg();
  TemporaryTypeSet* thisTypes = obj->resultTypeSet();
  if (!thisTypes) {
    return InliningStatus_NotInlined;
  }
  const JSClass* clasp = thisTypes->getKnownClass(constraints());
  if (clasp != &ArrayObject::class_) {
    return InliningStatus_NotInlined;
  }
  if (thisTypes->hasObjectFlags(constraints(), unhandledFlags)) {
    return InliningStatus_NotInlined;
  }

  // Watch out for extra indexed properties on the object or its prototype.
  bool hasIndexedProperty;
  MOZ_TRY_VAR(hasIndexedProperty,
              ElementAccessHasExtraIndexedProperty(this, obj));
  if (hasIndexedProperty) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  obj = addMaybeCopyElementsForWrite(obj, /* checkNative = */ false);

  TemporaryTypeSet* returnTypes = getInlineReturnTypeSet();
  bool needsHoleCheck =
      thisTypes->hasObjectFlags(constraints(), OBJECT_FLAG_NON_PACKED);
  bool maybeUndefined = returnTypes->hasType(TypeSet::UndefinedType());

  BarrierKind barrier = PropertyReadNeedsTypeBarrier(
      analysisContext, alloc(), constraints(), obj, nullptr, returnTypes);
  if (barrier != BarrierKind::NoBarrier) {
    returnType = MIRType::Value;
  }

  MArrayPopShift* ins =
      MArrayPopShift::New(alloc(), obj, mode, needsHoleCheck, maybeUndefined);
  current->add(ins);
  current->push(ins);
  ins->setResultType(returnType);

  MOZ_TRY(resumeAfter(ins));
  MOZ_TRY(pushTypeBarrier(ins, returnTypes, barrier));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineArrayJoin(CallInfo& callInfo) {
  if (callInfo.argc() != 1 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  if (getInlineReturnType() != MIRType::String) {
    return InliningStatus_NotInlined;
  }
  if (callInfo.thisArg()->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }
  if (callInfo.getArg(0)->type() != MIRType::String) {
    return InliningStatus_NotInlined;
  }

  // If we can confirm that the class is an array, the codegen
  // for MArrayJoin can be notified to check for common empty and one-item
  // arrays.
  bool optimizeForArray = ([&]() {
    TemporaryTypeSet* thisTypes = callInfo.thisArg()->resultTypeSet();
    if (!thisTypes) {
      return false;
    }

    const JSClass* clasp = thisTypes->getKnownClass(constraints());
    if (clasp != &ArrayObject::class_) {
      return false;
    }

    return true;
  })();

  callInfo.setImplicitlyUsedUnchecked();

  MArrayJoin* ins = MArrayJoin::New(alloc(), callInfo.thisArg(),
                                    callInfo.getArg(0), optimizeForArray);

  current->add(ins);
  current->push(ins);

  MOZ_TRY(resumeAfter(ins));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineArrayPush(CallInfo& callInfo) {
  const uint32_t inlineArgsLimit = 10;
  if (callInfo.argc() < 1 || callInfo.argc() > inlineArgsLimit ||
      callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  // XXX bug 1493903.
  if (callInfo.argc() != 1) {
    return InliningStatus_NotInlined;
  }

  MDefinition* obj = callInfo.thisArg();
  for (uint32_t i = 0; i < callInfo.argc(); i++) {
    MDefinition* value = callInfo.getArg(i);
    if (PropertyWriteNeedsTypeBarrier(alloc(), constraints(), current, &obj,
                                      nullptr, &value,
                                      /* canModify = */ false)) {
      return InliningStatus_NotInlined;
    }
  }

  if (getInlineReturnType() != MIRType::Int32) {
    return InliningStatus_NotInlined;
  }
  if (obj->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }

  TemporaryTypeSet* thisTypes = obj->resultTypeSet();
  if (!thisTypes) {
    return InliningStatus_NotInlined;
  }
  const JSClass* clasp = thisTypes->getKnownClass(constraints());
  if (clasp != &ArrayObject::class_) {
    return InliningStatus_NotInlined;
  }

  bool hasIndexedProperty;
  MOZ_TRY_VAR(hasIndexedProperty,
              ElementAccessHasExtraIndexedProperty(this, obj));
  if (hasIndexedProperty) {
    return InliningStatus_NotInlined;
  }

  TemporaryTypeSet::DoubleConversion conversion =
      thisTypes->convertDoubleElements(constraints());
  if (conversion == TemporaryTypeSet::AmbiguousDoubleConversion) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  bool toDouble = conversion == TemporaryTypeSet::AlwaysConvertToDoubles ||
                  conversion == TemporaryTypeSet::MaybeConvertToDoubles;

  obj = addMaybeCopyElementsForWrite(obj, /* checkNative = */ false);

  // If we have more than one argument, we are splitting the function into
  // multiple inlined calls to Array.push.
  //
  // Still, in case of bailouts, we have to keep the atomicity of the call, as
  // we cannot resume within Array.push function. To do so, we register a
  // resume point which would be captured by the upcoming MArrayPush
  // instructions, and this resume point contains an instruction which
  // truncates the length of the Array, to its original length in case of
  // bailouts, and resume before the Array.push call.
  MResumePoint* lastRp = nullptr;
  MInstruction* truncate = nullptr;
  if (callInfo.argc() > 1) {
    MInstruction* elements = MElements::New(alloc(), obj);
    MInstruction* length = MArrayLength::New(alloc(), elements);
    truncate = MSetArrayLength::New(alloc(), obj, length);
    truncate->setRecoveredOnBailout();

    current->add(elements);
    current->add(length);
    current->add(truncate);

    // Restore the stack, such that resume points are created with the stack
    // as it was before the call.
    MOZ_TRY(callInfo.pushPriorCallStack(&mirGen_, current));
  }

  MInstruction* ins = nullptr;
  for (uint32_t i = 0; i < callInfo.argc(); i++) {
    MDefinition* value = callInfo.getArg(i);
    if (toDouble) {
      MInstruction* valueDouble = MToDouble::New(alloc(), value);
      current->add(valueDouble);
      value = valueDouble;
    }

    if (needsPostBarrier(value)) {
      MInstruction* elements = MElements::New(alloc(), obj);
      current->add(elements);
      MInstruction* initLength = MInitializedLength::New(alloc(), elements);
      current->add(initLength);
      current->add(
          MPostWriteElementBarrier::New(alloc(), obj, value, initLength));
    }

    ins = MArrayPush::New(alloc(), obj, value);
    current->add(ins);

    if (callInfo.argc() > 1) {
      // Restore that call stack and the array length.
      MOZ_TRY(resumeAt(ins, pc));
      ins->resumePoint()->addStore(alloc(), truncate, lastRp);
      lastRp = ins->resumePoint();
    }
  }

  if (callInfo.argc() > 1) {
    // Fix the stack to represent the state after the call execution.
    callInfo.popPriorCallStack(current);
  }
  current->push(ins);

  if (callInfo.argc() > 1) {
    ins = MNop::New(alloc());
    current->add(ins);
  }

  MOZ_TRY(resumeAfter(ins));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineArraySlice(CallInfo& callInfo) {
  if (callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  MDefinition* obj = callInfo.thisArg();

  // Ensure |this| and result are objects.
  if (getInlineReturnType() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }
  if (obj->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }

  // Arguments for the sliced region must be integers.
  if (callInfo.argc() > 0) {
    if (callInfo.getArg(0)->type() != MIRType::Int32) {
      return InliningStatus_NotInlined;
    }
    if (callInfo.argc() > 1) {
      if (callInfo.getArg(1)->type() != MIRType::Int32) {
        return InliningStatus_NotInlined;
      }
    }
  }

  // |this| must be a dense array.
  TemporaryTypeSet* thisTypes = obj->resultTypeSet();
  if (!thisTypes) {
    return InliningStatus_NotInlined;
  }

  const JSClass* clasp = thisTypes->getKnownClass(constraints());
  if (clasp != &ArrayObject::class_) {
    return InliningStatus_NotInlined;
  }

  // Watch out for extra indexed properties on the object or its prototype.
  bool hasIndexedProperty;
  MOZ_TRY_VAR(hasIndexedProperty,
              ElementAccessHasExtraIndexedProperty(this, obj));
  if (hasIndexedProperty) {
    return InliningStatus_NotInlined;
  }

  // The group of the result will be dynamically fixed up to match the input
  // object, allowing us to handle 'this' objects that might have more than
  // one group. Make sure that no singletons can be sliced here.
  for (unsigned i = 0; i < thisTypes->getObjectCount(); i++) {
    TypeSet::ObjectKey* key = thisTypes->getObject(i);
    if (key && key->isSingleton()) {
      return InliningStatus_NotInlined;
    }
  }

  // Inline the call.
  JSObject* templateObj =
      inspector->getTemplateObjectForNative(pc, js::array_slice);
  if (!templateObj) {
    return InliningStatus_NotInlined;
  }

  if (!templateObj->is<ArrayObject>()) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MDefinition* begin;
  if (callInfo.argc() > 0) {
    begin = callInfo.getArg(0);
  } else {
    begin = constant(Int32Value(0));
  }

  MDefinition* end;
  if (callInfo.argc() > 1) {
    end = callInfo.getArg(1);
  } else if (clasp == &ArrayObject::class_) {
    MElements* elements = MElements::New(alloc(), obj);
    current->add(elements);

    end = MArrayLength::New(alloc(), elements);
    current->add(end->toInstruction());
  }

  MArraySlice* ins =
      MArraySlice::New(alloc(), obj, begin, end, templateObj,
                       templateObj->group()->initialHeap(constraints()));
  current->add(ins);
  current->push(ins);

  MOZ_TRY(resumeAfter(ins));
  MOZ_TRY(pushTypeBarrier(ins, getInlineReturnTypeSet(), BarrierKind::TypeSet));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineBoolean(CallInfo& callInfo) {
  if (callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  if (getInlineReturnType() != MIRType::Boolean) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  if (callInfo.argc() > 0) {
    MDefinition* result = convertToBoolean(callInfo.getArg(0));
    current->push(result);
  } else {
    pushConstant(BooleanValue(false));
  }
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineNewIterator(
    CallInfo& callInfo, MNewIterator::Type type) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 0);

  JSObject* templateObject = nullptr;
  switch (type) {
    case MNewIterator::ArrayIterator:
      templateObject = inspector->getTemplateObjectForNative(
          pc, js::intrinsic_NewArrayIterator);
      MOZ_ASSERT_IF(templateObject, templateObject->is<ArrayIteratorObject>());
      break;
    case MNewIterator::StringIterator:
      templateObject = inspector->getTemplateObjectForNative(
          pc, js::intrinsic_NewStringIterator);
      MOZ_ASSERT_IF(templateObject, templateObject->is<StringIteratorObject>());
      break;
    case MNewIterator::RegExpStringIterator:
      templateObject = inspector->getTemplateObjectForNative(
          pc, js::intrinsic_NewRegExpStringIterator);
      MOZ_ASSERT_IF(templateObject,
                    templateObject->is<RegExpStringIteratorObject>());
      break;
  }

  if (!templateObject) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MConstant* templateConst =
      MConstant::NewConstraintlessObject(alloc(), templateObject);
  current->add(templateConst);

  MNewIterator* ins =
      MNewIterator::New(alloc(), constraints(), templateConst, type);
  current->add(ins);
  current->push(ins);

  MOZ_TRY(resumeAfter(ins));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineArrayIteratorPrototypeOptimizable(
    CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 0);

  if (!ensureArrayIteratorPrototypeNextNotModified()) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();
  pushConstant(BooleanValue(true));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineMathAbs(CallInfo& callInfo) {
  if (callInfo.argc() != 1 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  MIRType returnType = getInlineReturnType();
  MIRType argType = callInfo.getArg(0)->type();
  if (!IsNumberType(argType)) {
    return InliningStatus_NotInlined;
  }

  // Either argType == returnType, or
  //        argType == Double or Float32, returnType == Int, or
  //        argType == Float32, returnType == Double
  if (argType != returnType &&
      !(IsFloatingPointType(argType) && returnType == MIRType::Int32) &&
      !(argType == MIRType::Float32 && returnType == MIRType::Double)) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  // If the arg is a Float32, we specialize the op as double, it will be
  // specialized as float32 if necessary later.
  MIRType absType = (argType == MIRType::Float32) ? MIRType::Double : argType;
  MInstruction* ins = MAbs::New(alloc(), callInfo.getArg(0), absType);
  current->add(ins);

  current->push(ins);
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineMathFloor(CallInfo& callInfo) {
  if (callInfo.argc() != 1 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  MIRType argType = callInfo.getArg(0)->type();
  MIRType returnType = getInlineReturnType();

  // Math.floor(int(x)) == int(x)
  if (argType == MIRType::Int32 && returnType == MIRType::Int32) {
    callInfo.setImplicitlyUsedUnchecked();
    // The int operand may be something which bails out if the actual value
    // is not in the range of the result type of the MIR. We need to tell
    // the optimizer to preserve this bailout even if the final result is
    // fully truncated.
    MLimitedTruncate* ins = MLimitedTruncate::New(
        alloc(), callInfo.getArg(0), MDefinition::IndirectTruncate);
    current->add(ins);
    current->push(ins);
    return InliningStatus_Inlined;
  }

  if (IsFloatingPointType(argType)) {
    if (returnType == MIRType::Int32) {
      callInfo.setImplicitlyUsedUnchecked();
      MFloor* ins = MFloor::New(alloc(), callInfo.getArg(0));
      current->add(ins);
      current->push(ins);
      return InliningStatus_Inlined;
    }

    if (returnType == MIRType::Double) {
      callInfo.setImplicitlyUsedUnchecked();

      MInstruction* ins = nullptr;
      if (MNearbyInt::HasAssemblerSupport(RoundingMode::Down)) {
        ins = MNearbyInt::New(alloc(), callInfo.getArg(0), argType,
                              RoundingMode::Down);
      } else {
        ins = MMathFunction::New(alloc(), callInfo.getArg(0),
                                 MMathFunction::Floor);
      }

      current->add(ins);
      current->push(ins);
      return InliningStatus_Inlined;
    }
  }

  return InliningStatus_NotInlined;
}

IonBuilder::InliningResult IonBuilder::inlineMathCeil(CallInfo& callInfo) {
  if (callInfo.argc() != 1 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  MIRType argType = callInfo.getArg(0)->type();
  MIRType returnType = getInlineReturnType();

  // Math.ceil(int(x)) == int(x)
  if (argType == MIRType::Int32 && returnType == MIRType::Int32) {
    callInfo.setImplicitlyUsedUnchecked();
    // The int operand may be something which bails out if the actual value
    // is not in the range of the result type of the MIR. We need to tell
    // the optimizer to preserve this bailout even if the final result is
    // fully truncated.
    MLimitedTruncate* ins = MLimitedTruncate::New(
        alloc(), callInfo.getArg(0), MDefinition::IndirectTruncate);
    current->add(ins);
    current->push(ins);
    return InliningStatus_Inlined;
  }

  if (IsFloatingPointType(argType)) {
    if (returnType == MIRType::Int32) {
      callInfo.setImplicitlyUsedUnchecked();
      MCeil* ins = MCeil::New(alloc(), callInfo.getArg(0));
      current->add(ins);
      current->push(ins);
      return InliningStatus_Inlined;
    }

    if (returnType == MIRType::Double) {
      callInfo.setImplicitlyUsedUnchecked();

      MInstruction* ins = nullptr;
      if (MNearbyInt::HasAssemblerSupport(RoundingMode::Up)) {
        ins = MNearbyInt::New(alloc(), callInfo.getArg(0), argType,
                              RoundingMode::Up);
      } else {
        ins = MMathFunction::New(alloc(), callInfo.getArg(0),
                                 MMathFunction::Ceil);
      }

      current->add(ins);
      current->push(ins);
      return InliningStatus_Inlined;
    }
  }

  return InliningStatus_NotInlined;
}

IonBuilder::InliningResult IonBuilder::inlineMathClz32(CallInfo& callInfo) {
  if (callInfo.argc() != 1 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  MIRType returnType = getInlineReturnType();
  if (returnType != MIRType::Int32) {
    return InliningStatus_NotInlined;
  }

  if (!IsNumberType(callInfo.getArg(0)->type())) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MClz* ins = MClz::New(alloc(), callInfo.getArg(0), MIRType::Int32);
  current->add(ins);
  current->push(ins);
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineMathRound(CallInfo& callInfo) {
  if (callInfo.argc() != 1 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  MIRType returnType = getInlineReturnType();
  MIRType argType = callInfo.getArg(0)->type();

  // Math.round(int(x)) == int(x)
  if (argType == MIRType::Int32 && returnType == MIRType::Int32) {
    callInfo.setImplicitlyUsedUnchecked();
    // The int operand may be something which bails out if the actual value
    // is not in the range of the result type of the MIR. We need to tell
    // the optimizer to preserve this bailout even if the final result is
    // fully truncated.
    MLimitedTruncate* ins = MLimitedTruncate::New(
        alloc(), callInfo.getArg(0), MDefinition::IndirectTruncate);
    current->add(ins);
    current->push(ins);
    return InliningStatus_Inlined;
  }

  if (IsFloatingPointType(argType) && returnType == MIRType::Int32) {
    callInfo.setImplicitlyUsedUnchecked();
    MRound* ins = MRound::New(alloc(), callInfo.getArg(0));
    current->add(ins);
    current->push(ins);
    return InliningStatus_Inlined;
  }

  if (IsFloatingPointType(argType) && returnType == MIRType::Double) {
    callInfo.setImplicitlyUsedUnchecked();
    MMathFunction* ins =
        MMathFunction::New(alloc(), callInfo.getArg(0), MMathFunction::Round);
    current->add(ins);
    current->push(ins);
    return InliningStatus_Inlined;
  }

  return InliningStatus_NotInlined;
}

IonBuilder::InliningResult IonBuilder::inlineMathSqrt(CallInfo& callInfo) {
  if (callInfo.argc() != 1 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  MIRType argType = callInfo.getArg(0)->type();
  if (getInlineReturnType() != MIRType::Double) {
    return InliningStatus_NotInlined;
  }
  if (!IsNumberType(argType)) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MSqrt* sqrt = MSqrt::New(alloc(), callInfo.getArg(0), MIRType::Double);
  current->add(sqrt);
  current->push(sqrt);
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineMathAtan2(CallInfo& callInfo) {
  if (callInfo.argc() != 2 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  if (getInlineReturnType() != MIRType::Double) {
    return InliningStatus_NotInlined;
  }

  MIRType argType0 = callInfo.getArg(0)->type();
  MIRType argType1 = callInfo.getArg(1)->type();

  if (!IsNumberType(argType0) || !IsNumberType(argType1)) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MAtan2* atan2 = MAtan2::New(alloc(), callInfo.getArg(0), callInfo.getArg(1));
  current->add(atan2);
  current->push(atan2);
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineMathHypot(CallInfo& callInfo) {
  if (callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  uint32_t argc = callInfo.argc();
  if (argc < 2 || argc > 4) {
    return InliningStatus_NotInlined;
  }

  if (getInlineReturnType() != MIRType::Double) {
    return InliningStatus_NotInlined;
  }

  MDefinitionVector vector(alloc());
  if (!vector.reserve(argc)) {
    return InliningStatus_NotInlined;
  }

  for (uint32_t i = 0; i < argc; ++i) {
    MDefinition* arg = callInfo.getArg(i);
    if (!IsNumberType(arg->type())) {
      return InliningStatus_NotInlined;
    }
    vector.infallibleAppend(arg);
  }

  callInfo.setImplicitlyUsedUnchecked();
  MHypot* hypot = MHypot::New(alloc(), vector);

  if (!hypot) {
    return InliningStatus_NotInlined;
  }

  current->add(hypot);
  current->push(hypot);
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineMathPow(CallInfo& callInfo) {
  if (callInfo.argc() != 2 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  bool emitted = false;
  MOZ_TRY(powTrySpecialized(&emitted, callInfo.getArg(0), callInfo.getArg(1),
                            getInlineReturnType()));

  if (!emitted) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineMathRandom(CallInfo& callInfo) {
  if (callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  if (getInlineReturnType() != MIRType::Double) {
    return InliningStatus_NotInlined;
  }

  // MRandom JIT code directly accesses the RNG. It's (barely) possible to
  // inline Math.random without it having been called yet, so ensure RNG
  // state that isn't guaranteed to be initialized already.
  script()->realm()->getOrCreateRandomNumberGenerator();

  callInfo.setImplicitlyUsedUnchecked();

  MRandom* rand = MRandom::New(alloc());
  current->add(rand);
  current->push(rand);
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineMathImul(CallInfo& callInfo) {
  if (callInfo.argc() != 2 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  MIRType returnType = getInlineReturnType();
  if (returnType != MIRType::Int32) {
    return InliningStatus_NotInlined;
  }

  if (!IsNumberType(callInfo.getArg(0)->type())) {
    return InliningStatus_NotInlined;
  }
  if (!IsNumberType(callInfo.getArg(1)->type())) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MInstruction* first = MTruncateToInt32::New(alloc(), callInfo.getArg(0));
  current->add(first);

  MInstruction* second = MTruncateToInt32::New(alloc(), callInfo.getArg(1));
  current->add(second);

  MMul* ins = MMul::New(alloc(), first, second, MIRType::Int32, MMul::Integer);
  current->add(ins);
  current->push(ins);
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineMathFRound(CallInfo& callInfo) {
  if (callInfo.argc() != 1 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  // MIRType can't be Float32, as this point, as getInlineReturnType uses JSVal
  // types to infer the returned MIR type.
  TemporaryTypeSet* returned = getInlineReturnTypeSet();
  if (returned->empty()) {
    // As there's only one possible returned type, just add it to the observed
    // returned typeset
    returned->addType(TypeSet::DoubleType(), alloc_->lifoAlloc());
  } else {
    MIRType returnType = getInlineReturnType();
    if (!IsNumberType(returnType)) {
      return InliningStatus_NotInlined;
    }
  }

  MIRType arg = callInfo.getArg(0)->type();
  if (!IsNumberType(arg)) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MToFloat32* ins = MToFloat32::New(alloc(), callInfo.getArg(0));
  current->add(ins);
  current->push(ins);
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineMathTrunc(CallInfo& callInfo) {
  if (callInfo.argc() != 1 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  MIRType argType = callInfo.getArg(0)->type();
  MIRType returnType = getInlineReturnType();

  // Math.trunc(int(x)) == int(x)
  if (argType == MIRType::Int32 && returnType == MIRType::Int32) {
    callInfo.setImplicitlyUsedUnchecked();
    // The int operand may be something which bails out if the actual value
    // is not in the range of the result type of the MIR. We need to tell
    // the optimizer to preserve this bailout even if the final result is
    // fully truncated.
    MLimitedTruncate* ins = MLimitedTruncate::New(
        alloc(), callInfo.getArg(0), MDefinition::IndirectTruncate);
    current->add(ins);
    current->push(ins);
    return InliningStatus_Inlined;
  }

  if (IsFloatingPointType(argType)) {
    if (returnType == MIRType::Int32) {
      callInfo.setImplicitlyUsedUnchecked();
      MTrunc* ins = MTrunc::New(alloc(), callInfo.getArg(0));
      current->add(ins);
      current->push(ins);
      return InliningStatus_Inlined;
    }

    if (returnType == MIRType::Double) {
      callInfo.setImplicitlyUsedUnchecked();

      MInstruction* ins = nullptr;
      if (MNearbyInt::HasAssemblerSupport(RoundingMode::TowardsZero)) {
        ins = MNearbyInt::New(alloc(), callInfo.getArg(0), argType,
                              RoundingMode::TowardsZero);
      } else {
        ins = MMathFunction::New(alloc(), callInfo.getArg(0),
                                 MMathFunction::Trunc);
      }

      current->add(ins);
      current->push(ins);
      return InliningStatus_Inlined;
    }
  }

  return InliningStatus_NotInlined;
}

IonBuilder::InliningResult IonBuilder::inlineMathSign(CallInfo& callInfo) {
  if (callInfo.argc() != 1 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  MIRType argType = callInfo.getArg(0)->type();
  MIRType returnType = getInlineReturnType();

  if (returnType != MIRType::Int32 && returnType != MIRType::Double) {
    return InliningStatus_NotInlined;
  }

  if (!IsFloatingPointType(argType) &&
      !(argType == MIRType::Int32 && returnType == MIRType::Int32)) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  auto* ins = MSign::New(alloc(), callInfo.getArg(0), returnType);
  current->add(ins);
  current->push(ins);

  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineMathMinMax(CallInfo& callInfo,
                                                        bool max) {
  if (callInfo.argc() < 1 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  MIRType returnType = getInlineReturnType();
  if (!IsNumberType(returnType)) {
    return InliningStatus_NotInlined;
  }

  MDefinitionVector int32_cases(alloc());
  for (unsigned i = 0; i < callInfo.argc(); i++) {
    MDefinition* arg = callInfo.getArg(i);

    switch (arg->type()) {
      case MIRType::Int32:
        if (!int32_cases.append(arg)) {
          return abort(AbortReason::Alloc);
        }
        break;
      case MIRType::Double:
      case MIRType::Float32:
        // Don't force a double MMinMax for arguments that would be a NOP
        // when doing an integer MMinMax.
        if (arg->isConstant()) {
          double cte = arg->toConstant()->numberToDouble();
          // min(int32, cte >= INT32_MAX) = int32
          if (cte >= INT32_MAX && !max) {
            break;
          }
          // max(int32, cte <= INT32_MIN) = int32
          if (cte <= INT32_MIN && max) {
            break;
          }
        }

        // Force double MMinMax if argument is a "effectfull" double.
        returnType = MIRType::Double;
        break;
      default:
        return InliningStatus_NotInlined;
    }
  }

  if (int32_cases.length() == 0) {
    returnType = MIRType::Double;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MDefinitionVector& cases =
      (returnType == MIRType::Int32) ? int32_cases : callInfo.argv();

  if (cases.length() == 1) {
    MLimitedTruncate* limit =
        MLimitedTruncate::New(alloc(), cases[0], MDefinition::NoTruncate);
    current->add(limit);
    current->push(limit);
    return InliningStatus_Inlined;
  }

  // Chain N-1 MMinMax instructions to compute the MinMax.
  MMinMax* last = MMinMax::New(alloc(), cases[0], cases[1], returnType, max);
  current->add(last);

  for (unsigned i = 2; i < cases.length(); i++) {
    MMinMax* ins =
        MMinMax::New(alloc().fallible(), last, cases[i], returnType, max);
    if (!ins) {
      return abort(AbortReason::Alloc);
    }
    current->add(ins);
    last = ins;
  }

  current->push(last);
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineStringObject(CallInfo& callInfo) {
  if (callInfo.argc() != 1 || !callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  // ConvertToString doesn't support objects.
  if (callInfo.getArg(0)->mightBeType(MIRType::Object)) {
    return InliningStatus_NotInlined;
  }

  JSObject* templateObj =
      inspector->getTemplateObjectForNative(pc, StringConstructor);
  if (!templateObj) {
    return InliningStatus_NotInlined;
  }
  MOZ_ASSERT(templateObj->is<StringObject>());

  callInfo.setImplicitlyUsedUnchecked();

  MNewStringObject* ins =
      MNewStringObject::New(alloc(), callInfo.getArg(0), templateObj);
  current->add(ins);
  current->push(ins);

  MOZ_TRY(resumeAfter(ins));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineStringSplitString(
    CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 2);

  MDefinition* strArg = callInfo.getArg(0);
  MDefinition* sepArg = callInfo.getArg(1);

  if (strArg->type() != MIRType::String) {
    return InliningStatus_NotInlined;
  }

  if (sepArg->type() != MIRType::String) {
    return InliningStatus_NotInlined;
  }

  JSContext* cx = TlsContext.get();
  ObjectGroup* group = ObjectGroupRealm::getStringSplitStringGroup(cx);
  if (!group) {
    return InliningStatus_NotInlined;
  }

  TypeSet::ObjectKey* retKey = TypeSet::ObjectKey::get(group);
  if (retKey->unknownProperties()) {
    return InliningStatus_NotInlined;
  }

  HeapTypeSetKey key = retKey->property(JSID_VOID);
  if (!key.maybeTypes()) {
    return InliningStatus_NotInlined;
  }

  if (!key.maybeTypes()->hasType(TypeSet::StringType())) {
    key.freeze(constraints());
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();
  MStringSplit* ins =
      MStringSplit::New(alloc(), constraints(), strArg, sepArg, group);
  current->add(ins);
  current->push(ins);

  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineObjectHasPrototype(
    CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 2);

  MDefinition* objArg = callInfo.getArg(0);
  MDefinition* protoArg = callInfo.getArg(1);

  if (objArg->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }
  if (protoArg->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }

  // Inline only when both obj and proto are singleton objects and
  // obj does not have uncacheable proto and obj.__proto__ is proto.
  TemporaryTypeSet* objTypes = objArg->resultTypeSet();
  if (!objTypes || objTypes->unknownObject() ||
      objTypes->getObjectCount() != 1) {
    return InliningStatus_NotInlined;
  }

  TypeSet::ObjectKey* objKey = objTypes->getObject(0);
  if (!objKey || !objKey->hasStableClassAndProto(constraints())) {
    return InliningStatus_NotInlined;
  }
  if (!objKey->isSingleton() || !objKey->singleton()->is<NativeObject>()) {
    return InliningStatus_NotInlined;
  }

  JSObject* obj = &objKey->singleton()->as<NativeObject>();
  if (obj->hasUncacheableProto()) {
    return InliningStatus_NotInlined;
  }

  JSObject* actualProto = checkNurseryObject(objKey->proto().toObjectOrNull());
  if (actualProto == nullptr) {
    return InliningStatus_NotInlined;
  }

  TemporaryTypeSet* protoTypes = protoArg->resultTypeSet();
  if (!protoTypes || protoTypes->unknownObject() ||
      protoTypes->getObjectCount() != 1) {
    return InliningStatus_NotInlined;
  }

  TypeSet::ObjectKey* protoKey = protoTypes->getObject(0);
  if (!protoKey || !protoKey->hasStableClassAndProto(constraints())) {
    return InliningStatus_NotInlined;
  }
  if (!protoKey->isSingleton() || !protoKey->singleton()->is<NativeObject>()) {
    return InliningStatus_NotInlined;
  }

  JSObject* proto = &protoKey->singleton()->as<NativeObject>();
  pushConstant(BooleanValue(proto == actualProto));
  callInfo.setImplicitlyUsedUnchecked();
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineFinishBoundFunctionInit(
    CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 3);
  MOZ_ASSERT(BytecodeIsPopped(pc));

  MDefinition* boundFunction = callInfo.getArg(0);
  MDefinition* targetFunction = callInfo.getArg(1);
  MDefinition* argCount = callInfo.getArg(2);

  if (boundFunction->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }
  if (targetFunction->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }
  if (argCount->type() != MIRType::Int32) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  auto* ins = MFinishBoundFunctionInit::New(alloc(), boundFunction,
                                            targetFunction, argCount);
  current->add(ins);

  pushConstant(UndefinedValue());

  MOZ_TRY(resumeAfter(ins));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineIsPackedArray(CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 1);

  if (getInlineReturnType() != MIRType::Boolean) {
    return InliningStatus_NotInlined;
  }

  MDefinition* array = callInfo.getArg(0);

  if (array->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }

  TemporaryTypeSet* arrayTypes = array->resultTypeSet();
  if (!arrayTypes) {
    return InliningStatus_NotInlined;
  }

  const JSClass* clasp = arrayTypes->getKnownClass(constraints());
  if (clasp != &ArrayObject::class_) {
    return InliningStatus_NotInlined;
  }

  // Only inline if the array uses dense storage.
  ObjectGroupFlags unhandledFlags = OBJECT_FLAG_SPARSE_INDEXES |
                                    OBJECT_FLAG_LENGTH_OVERFLOW |
                                    OBJECT_FLAG_NON_PACKED;

  if (arrayTypes->hasObjectFlags(constraints(), unhandledFlags)) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  auto* ins = MIsPackedArray::New(alloc(), array);
  current->add(ins);
  current->push(ins);

  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineReflectGetPrototypeOf(
    CallInfo& callInfo) {
  if (callInfo.argc() != 1 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  MDefinition* target = callInfo.getArg(0);
  if (target->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  auto* ins = MGetPrototypeOf::New(alloc(), target);
  current->add(ins);
  current->push(ins);

  MOZ_TRY(resumeAfter(ins));
  MOZ_TRY(pushTypeBarrier(ins, getInlineReturnTypeSet(), BarrierKind::TypeSet));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineStrCharCodeAt(CallInfo& callInfo) {
  if (callInfo.argc() != 1 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  if (getInlineReturnType() != MIRType::Int32) {
    return InliningStatus_NotInlined;
  }
  if (callInfo.thisArg()->type() != MIRType::String &&
      callInfo.thisArg()->type() != MIRType::Value) {
    return InliningStatus_NotInlined;
  }
  MIRType argType = callInfo.getArg(0)->type();
  if (argType != MIRType::Int32 && argType != MIRType::Double) {
    return InliningStatus_NotInlined;
  }

  // Check for STR.charCodeAt(IDX) where STR is a constant string and IDX is a
  // constant integer.
  InliningStatus constInlineStatus;
  MOZ_TRY_VAR(constInlineStatus, inlineConstantCharCodeAt(callInfo));
  if (constInlineStatus != InliningStatus_NotInlined) {
    return constInlineStatus;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MInstruction* index = MToIntegerInt32::New(alloc(), callInfo.getArg(0));
  current->add(index);

  MStringLength* length = MStringLength::New(alloc(), callInfo.thisArg());
  current->add(length);

  index = addBoundsCheck(index, length);

  MCharCodeAt* charCode = MCharCodeAt::New(alloc(), callInfo.thisArg(), index);
  current->add(charCode);
  current->push(charCode);
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineConstantCharCodeAt(
    CallInfo& callInfo) {
  if (!callInfo.thisArg()->maybeConstantValue() ||
      !callInfo.getArg(0)->maybeConstantValue()) {
    return InliningStatus_NotInlined;
  }

  MConstant* strval = callInfo.thisArg()->maybeConstantValue();
  MConstant* idxval = callInfo.getArg(0)->maybeConstantValue();

  if (strval->type() != MIRType::String || idxval->type() != MIRType::Int32) {
    return InliningStatus_NotInlined;
  }

  JSString* str = strval->toString();
  if (!str->isLinear()) {
    return InliningStatus_NotInlined;
  }

  int32_t idx = idxval->toInt32();
  if (idx < 0 || (uint32_t(idx) >= str->length())) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  JSLinearString& linstr = str->asLinear();
  char16_t ch = linstr.latin1OrTwoByteChar(idx);
  MConstant* result = MConstant::New(alloc(), Int32Value(ch));
  current->add(result);
  current->push(result);
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineStrFromCharCode(
    CallInfo& callInfo) {
  if (callInfo.argc() != 1 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  if (getInlineReturnType() != MIRType::String) {
    return InliningStatus_NotInlined;
  }

  MDefinition* codeUnit = callInfo.getArg(0);
  if (codeUnit->type() != MIRType::Int32) {
    // MTruncateToInt32 will always bail for objects, symbols and BigInts, so
    // don't try to inline String.fromCharCode() for these value types.
    if (codeUnit->mightBeType(MIRType::Object) ||
        codeUnit->mightBeType(MIRType::Symbol) ||
        codeUnit->mightBeType(MIRType::BigInt)) {
      return InliningStatus_NotInlined;
    }

    codeUnit = MTruncateToInt32::New(alloc(), codeUnit);
    current->add(codeUnit->toInstruction());
  }

  callInfo.setImplicitlyUsedUnchecked();

  MFromCharCode* string = MFromCharCode::New(alloc(), codeUnit);
  current->add(string);
  current->push(string);
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineStrFromCodePoint(
    CallInfo& callInfo) {
  if (callInfo.argc() != 1 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  if (getInlineReturnType() != MIRType::String) {
    return InliningStatus_NotInlined;
  }
  MIRType argType = callInfo.getArg(0)->type();
  if (argType != MIRType::Int32 && argType != MIRType::Double) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  auto* codePoint = MToNumberInt32::New(alloc(), callInfo.getArg(0));
  current->add(codePoint);

  MFromCodePoint* string = MFromCodePoint::New(alloc(), codePoint);
  current->add(string);
  current->push(string);
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineStrCharAt(CallInfo& callInfo) {
  if (callInfo.argc() != 1 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  if (getInlineReturnType() != MIRType::String) {
    return InliningStatus_NotInlined;
  }
  if (callInfo.thisArg()->type() != MIRType::String) {
    return InliningStatus_NotInlined;
  }
  MIRType argType = callInfo.getArg(0)->type();
  if (argType != MIRType::Int32 && argType != MIRType::Double) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MInstruction* index = MToIntegerInt32::New(alloc(), callInfo.getArg(0));
  current->add(index);

  MStringLength* length = MStringLength::New(alloc(), callInfo.thisArg());
  current->add(length);

  index = addBoundsCheck(index, length);

  // String.charAt(x) = String.fromCharCode(String.charCodeAt(x))
  MCharCodeAt* charCode = MCharCodeAt::New(alloc(), callInfo.thisArg(), index);
  current->add(charCode);

  MFromCharCode* string = MFromCharCode::New(alloc(), charCode);
  current->add(string);
  current->push(string);
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineStringConvertCase(
    CallInfo& callInfo, MStringConvertCase::Mode mode) {
  if (callInfo.argc() != 0 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  if (getInlineReturnType() != MIRType::String) {
    return InliningStatus_NotInlined;
  }
  if (callInfo.thisArg()->type() != MIRType::String) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  auto* ins = MStringConvertCase::New(alloc(), callInfo.thisArg(), mode);
  current->add(ins);
  current->push(ins);
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineRegExpMatcher(CallInfo& callInfo) {
  // This is called from Self-hosted JS, after testing each argument,
  // most of following tests should be passed.

  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 3);

  MDefinition* rxArg = callInfo.getArg(0);
  MDefinition* strArg = callInfo.getArg(1);
  MDefinition* lastIndexArg = callInfo.getArg(2);

  if (rxArg->type() != MIRType::Object && rxArg->type() != MIRType::Value) {
    return InliningStatus_NotInlined;
  }

  TemporaryTypeSet* rxTypes = rxArg->resultTypeSet();
  const JSClass* clasp =
      rxTypes ? rxTypes->getKnownClass(constraints()) : nullptr;
  if (clasp != &RegExpObject::class_) {
    return InliningStatus_NotInlined;
  }

  if (strArg->type() != MIRType::String && strArg->type() != MIRType::Value) {
    return InliningStatus_NotInlined;
  }

  if (lastIndexArg->type() != MIRType::Int32) {
    return InliningStatus_NotInlined;
  }

  JSContext* cx = TlsContext.get();
  if (!cx->realm()->jitRealm()->ensureRegExpMatcherStubExists(cx)) {
    cx->clearPendingException();  // OOM or overrecursion.
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MInstruction* matcher =
      MRegExpMatcher::New(alloc(), rxArg, strArg, lastIndexArg);
  current->add(matcher);
  current->push(matcher);

  MOZ_TRY(resumeAfter(matcher));
  MOZ_TRY(
      pushTypeBarrier(matcher, getInlineReturnTypeSet(), BarrierKind::TypeSet));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineRegExpSearcher(
    CallInfo& callInfo) {
  // This is called from Self-hosted JS, after testing each argument,
  // most of following tests should be passed.

  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 3);

  MDefinition* rxArg = callInfo.getArg(0);
  MDefinition* strArg = callInfo.getArg(1);
  MDefinition* lastIndexArg = callInfo.getArg(2);

  if (rxArg->type() != MIRType::Object && rxArg->type() != MIRType::Value) {
    return InliningStatus_NotInlined;
  }

  TemporaryTypeSet* regexpTypes = rxArg->resultTypeSet();
  const JSClass* clasp =
      regexpTypes ? regexpTypes->getKnownClass(constraints()) : nullptr;
  if (clasp != &RegExpObject::class_) {
    return InliningStatus_NotInlined;
  }

  if (strArg->type() != MIRType::String && strArg->type() != MIRType::Value) {
    return InliningStatus_NotInlined;
  }

  if (lastIndexArg->type() != MIRType::Int32) {
    return InliningStatus_NotInlined;
  }

  JSContext* cx = TlsContext.get();
  if (!cx->realm()->jitRealm()->ensureRegExpSearcherStubExists(cx)) {
    cx->clearPendingException();  // OOM or overrecursion.
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MInstruction* searcher =
      MRegExpSearcher::New(alloc(), rxArg, strArg, lastIndexArg);
  current->add(searcher);
  current->push(searcher);

  MOZ_TRY(resumeAfter(searcher));
  MOZ_TRY(pushTypeBarrier(searcher, getInlineReturnTypeSet(),
                          BarrierKind::TypeSet));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineRegExpTester(CallInfo& callInfo) {
  // This is called from Self-hosted JS, after testing each argument,
  // most of following tests should be passed.

  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 3);

  MDefinition* rxArg = callInfo.getArg(0);
  MDefinition* strArg = callInfo.getArg(1);
  MDefinition* lastIndexArg = callInfo.getArg(2);

  if (rxArg->type() != MIRType::Object && rxArg->type() != MIRType::Value) {
    return InliningStatus_NotInlined;
  }

  TemporaryTypeSet* rxTypes = rxArg->resultTypeSet();
  const JSClass* clasp =
      rxTypes ? rxTypes->getKnownClass(constraints()) : nullptr;
  if (clasp != &RegExpObject::class_) {
    return InliningStatus_NotInlined;
  }

  if (strArg->type() != MIRType::String && strArg->type() != MIRType::Value) {
    return InliningStatus_NotInlined;
  }

  if (lastIndexArg->type() != MIRType::Int32) {
    return InliningStatus_NotInlined;
  }

  JSContext* cx = TlsContext.get();
  if (!cx->realm()->jitRealm()->ensureRegExpTesterStubExists(cx)) {
    cx->clearPendingException();  // OOM or overrecursion.
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MInstruction* tester =
      MRegExpTester::New(alloc(), rxArg, strArg, lastIndexArg);
  current->add(tester);
  current->push(tester);

  MOZ_TRY(resumeAfter(tester));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineIsRegExpObject(
    CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 1);

  if (getInlineReturnType() != MIRType::Boolean) {
    return InliningStatus_NotInlined;
  }

  MDefinition* arg = callInfo.getArg(0);

  bool isRegExpObjectKnown = false;
  bool isRegExpObjectConstant;
  if (arg->type() == MIRType::Object) {
    TemporaryTypeSet* types = arg->resultTypeSet();
    const JSClass* clasp =
        types ? types->getKnownClass(constraints()) : nullptr;
    if (clasp) {
      isRegExpObjectKnown = true;
      isRegExpObjectConstant = (clasp == &RegExpObject::class_);
    }
  } else if (!arg->mightBeType(MIRType::Object)) {
    // NB: This case only happens when Phi-nodes flow into IsRegExpObject.
    // IsRegExpObject itself will never be called with a non-Object value.
    isRegExpObjectKnown = true;
    isRegExpObjectConstant = false;
  } else if (arg->type() != MIRType::Value) {
    return InliningStatus_NotInlined;
  }

  if (isRegExpObjectKnown) {
    pushConstant(BooleanValue(isRegExpObjectConstant));
  } else {
    MHasClass* hasClass = MHasClass::New(alloc(), arg, &RegExpObject::class_);
    current->add(hasClass);
    current->push(hasClass);
  }

  callInfo.setImplicitlyUsedUnchecked();
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineIsPossiblyWrappedRegExpObject(
    CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 1);

  if (getInlineReturnType() != MIRType::Boolean) {
    return InliningStatus_NotInlined;
  }

  MDefinition* arg = callInfo.getArg(0);
  if (arg->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }

  TemporaryTypeSet* types = arg->resultTypeSet();
  if (!types) {
    return InliningStatus_NotInlined;
  }

  // Don't inline if the argument might be a wrapper.
  if (types->forAllClasses(constraints(), IsProxyClass) !=
      TemporaryTypeSet::ForAllResult::ALL_FALSE) {
    return InliningStatus_NotInlined;
  }

  if (const JSClass* clasp = types->getKnownClass(constraints())) {
    pushConstant(BooleanValue(clasp == &RegExpObject::class_));
  } else {
    MHasClass* hasClass = MHasClass::New(alloc(), arg, &RegExpObject::class_);
    current->add(hasClass);
    current->push(hasClass);
  }

  callInfo.setImplicitlyUsedUnchecked();
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineRegExpPrototypeOptimizable(
    CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 1);

  MDefinition* protoArg = callInfo.getArg(0);

  if (protoArg->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }

  if (getInlineReturnType() != MIRType::Boolean) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MInstruction* opt = MRegExpPrototypeOptimizable::New(alloc(), protoArg);
  current->add(opt);
  current->push(opt);

  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineRegExpInstanceOptimizable(
    CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 2);

  MDefinition* rxArg = callInfo.getArg(0);
  MDefinition* protoArg = callInfo.getArg(1);

  if (rxArg->type() != MIRType::Object && rxArg->type() != MIRType::Value) {
    return InliningStatus_NotInlined;
  }

  if (protoArg->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }

  if (getInlineReturnType() != MIRType::Boolean) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MInstruction* opt = MRegExpInstanceOptimizable::New(alloc(), rxArg, protoArg);
  current->add(opt);
  current->push(opt);

  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineGetFirstDollarIndex(
    CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 1);

  MDefinition* strArg = callInfo.getArg(0);

  if (strArg->type() != MIRType::String) {
    return InliningStatus_NotInlined;
  }

  if (getInlineReturnType() != MIRType::Int32) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MInstruction* ins = MGetFirstDollarIndex::New(alloc(), strArg);
  current->add(ins);
  current->push(ins);

  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineStringReplaceString(
    CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 3);

  if (getInlineReturnType() != MIRType::String) {
    return InliningStatus_NotInlined;
  }

  MDefinition* strArg = callInfo.getArg(0);
  MDefinition* patArg = callInfo.getArg(1);
  MDefinition* replArg = callInfo.getArg(2);

  if (strArg->type() != MIRType::String) {
    return InliningStatus_NotInlined;
  }

  if (patArg->type() != MIRType::String) {
    return InliningStatus_NotInlined;
  }

  if (replArg->type() != MIRType::String) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MInstruction* cte = MStringReplace::New(alloc(), strArg, patArg, replArg);
  current->add(cte);
  current->push(cte);
  if (cte->isEffectful()) {
    MOZ_TRY(resumeAfter(cte));
  }
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineSubstringKernel(
    CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 3);

  // Return: String.
  if (getInlineReturnType() != MIRType::String) {
    return InliningStatus_NotInlined;
  }

  // Arg 0: String.
  if (callInfo.getArg(0)->type() != MIRType::String) {
    return InliningStatus_NotInlined;
  }

  // Arg 1: Int.
  if (callInfo.getArg(1)->type() != MIRType::Int32) {
    return InliningStatus_NotInlined;
  }

  // Arg 2: Int.
  if (callInfo.getArg(2)->type() != MIRType::Int32) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MSubstr* substr = MSubstr::New(alloc(), callInfo.getArg(0),
                                 callInfo.getArg(1), callInfo.getArg(2));
  current->add(substr);
  current->push(substr);

  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineObject(CallInfo& callInfo) {
  if (callInfo.argc() != 1 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  if (getInlineReturnType() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }

  MDefinition* arg = callInfo.getArg(0);
  if (arg->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();
  current->push(arg);

  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineObjectCreate(CallInfo& callInfo) {
  if (callInfo.argc() != 1 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  JSObject* templateObject =
      inspector->getTemplateObjectForNative(pc, obj_create);
  if (!templateObject) {
    return InliningStatus_NotInlined;
  }

  MOZ_ASSERT(templateObject->is<PlainObject>());
  MOZ_ASSERT(!templateObject->isSingleton());

  // Ensure the argument matches the template object's prototype.
  MDefinition* arg = callInfo.getArg(0);
  if (JSObject* proto = templateObject->staticPrototype()) {
    if (IsInsideNursery(proto)) {
      return InliningStatus_NotInlined;
    }

    TemporaryTypeSet* types = arg->resultTypeSet();
    if (!types || types->maybeSingleton() != proto) {
      return InliningStatus_NotInlined;
    }

    MOZ_ASSERT(types->getKnownMIRType() == MIRType::Object);
  } else {
    if (arg->type() != MIRType::Null) {
      return InliningStatus_NotInlined;
    }
  }

  callInfo.setImplicitlyUsedUnchecked();

  bool emitted = false;
  MOZ_TRY(newObjectTryTemplateObject(&emitted, templateObject));

  MOZ_ASSERT(emitted);
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineObjectIs(CallInfo& callInfo) {
  if (callInfo.argc() < 2 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  if (getInlineReturnType() != MIRType::Boolean) {
    return InliningStatus_NotInlined;
  }

  MDefinition* left = callInfo.getArg(0);
  MDefinition* right = callInfo.getArg(1);
  MIRType leftType = left->type();
  MIRType rightType = right->type();

  auto mightBeFloatingPointType = [](MDefinition* def) {
    return def->mightBeType(MIRType::Double) ||
           def->mightBeType(MIRType::Float32);
  };

  bool strictEq;
  bool incompatibleTypes = false;
  if (leftType == rightType) {
    // We can only compare the arguments with strict-equals semantics if
    // they aren't floating-point types or values which might be floating-
    // point types. Otherwise we need to use MSameValue.
    strictEq = leftType != MIRType::Value ? !IsFloatingPointType(leftType)
                                          : (!mightBeFloatingPointType(left) &&
                                             !mightBeFloatingPointType(right));
  } else if (leftType == MIRType::Value) {
    // Also use strict-equals when comparing a value with a non-number or
    // the value cannot be a floating-point type.
    strictEq = !IsNumberType(rightType) || !mightBeFloatingPointType(left);
  } else if (rightType == MIRType::Value) {
    // Dual case to the previous one, only with reversed operands.
    strictEq = !IsNumberType(leftType) || !mightBeFloatingPointType(right);
  } else if (IsNumberType(leftType) && IsNumberType(rightType)) {
    // Both arguments are numbers, but with different representations. We
    // can't use strict-equals semantics to compare the operands, but
    // instead need to use MSameValue.
    strictEq = false;
  } else {
    incompatibleTypes = true;
  }

  if (incompatibleTypes) {
    // The result is always |false| when comparing incompatible types.
    pushConstant(BooleanValue(false));
  } else if (strictEq) {
    // Specialize |Object.is(lhs, rhs)| as |lhs === rhs|.
    MOZ_TRY(jsop_compare(JSOp::StrictEq, left, right));
  } else {
    MSameValue* ins = MSameValue::New(alloc(), left, right);

    // The more specific operand is expected to be in the rhs.
    if (IsNumberType(leftType) && rightType == MIRType::Value) {
      ins->swapOperands();
    }

    current->add(ins);
    current->push(ins);
  }

  callInfo.setImplicitlyUsedUnchecked();
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineObjectToString(
    CallInfo& callInfo) {
  if (callInfo.constructing() || callInfo.argc() != 0) {
    return InliningStatus_NotInlined;
  }

  if (getInlineReturnType() != MIRType::String) {
    return InliningStatus_NotInlined;
  }

  MDefinition* arg = callInfo.thisArg();
  if (arg->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }

  TemporaryTypeSet* types = arg->resultTypeSet();
  if (!types || types->unknownObject()) {
    return InliningStatus_NotInlined;
  }

  // Don't optimize if this might be a proxy.
  using ForAllResult = TemporaryTypeSet::ForAllResult;
  if (types->forAllClasses(constraints(), IsProxyClass) !=
      ForAllResult::ALL_FALSE) {
    return InliningStatus_NotInlined;
  }

  // Make sure there's no Symbol.toStringTag property.
  jsid toStringTag =
      SYMBOL_TO_JSID(realm->runtime()->wellKnownSymbols().toStringTag);
  bool res;
  MOZ_TRY_VAR(res, testNotDefinedProperty(arg, toStringTag));
  if (!res) {
    return InliningStatus_NotInlined;
  }

  // At this point we know we're going to inline this.
  callInfo.setImplicitlyUsedUnchecked();

  // Try to constant fold some common cases.
  if (const JSClass* knownClass = types->getKnownClass(constraints())) {
    if (knownClass == &PlainObject::class_) {
      pushConstant(StringValue(names().objectObject));
      return InliningStatus_Inlined;
    }
    if (IsArrayClass(knownClass)) {
      pushConstant(StringValue(names().objectArray));
      return InliningStatus_Inlined;
    }
    if (knownClass == &JSFunction::class_) {
      pushConstant(StringValue(names().objectFunction));
      return InliningStatus_Inlined;
    }
  }

  MObjectClassToString* toString = MObjectClassToString::New(alloc(), arg);
  current->add(toString);
  current->push(toString);

  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineHasClass(CallInfo& callInfo,
                                                      const JSClass* clasp1,
                                                      const JSClass* clasp2,
                                                      const JSClass* clasp3,
                                                      const JSClass* clasp4) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 1);

  if (callInfo.getArg(0)->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }
  if (getInlineReturnType() != MIRType::Boolean) {
    return InliningStatus_NotInlined;
  }

  TemporaryTypeSet* types = callInfo.getArg(0)->resultTypeSet();
  const JSClass* knownClass =
      types ? types->getKnownClass(constraints()) : nullptr;
  if (knownClass) {
    pushConstant(BooleanValue(knownClass == clasp1 || knownClass == clasp2 ||
                              knownClass == clasp3 || knownClass == clasp4));
  } else {
    MHasClass* hasClass1 = MHasClass::New(alloc(), callInfo.getArg(0), clasp1);
    current->add(hasClass1);

    if (!clasp2 && !clasp3 && !clasp4) {
      current->push(hasClass1);
    } else {
      const JSClass* remaining[] = {clasp2, clasp3, clasp4};
      MDefinition* last = hasClass1;
      for (size_t i = 0; i < ArrayLength(remaining); i++) {
        MHasClass* hasClass =
            MHasClass::New(alloc(), callInfo.getArg(0), remaining[i]);
        current->add(hasClass);
        MBitOr* either = MBitOr::New(alloc(), last, hasClass, MIRType::Int32);
        current->add(either);
        last = either;
      }

      MDefinition* result = convertToBoolean(last);
      current->push(result);
    }
  }

  callInfo.setImplicitlyUsedUnchecked();
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineGuardToClass(
    CallInfo& callInfo, const JSClass* clasp) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 1);

  if (callInfo.getArg(0)->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }

  if (getInlineReturnType() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }

  TemporaryTypeSet* types = callInfo.getArg(0)->resultTypeSet();
  const JSClass* knownClass =
      types ? types->getKnownClass(constraints()) : nullptr;

  if (knownClass && knownClass == clasp) {
    current->push(callInfo.getArg(0));
  } else {
    MGuardToClass* guardToClass =
        MGuardToClass::New(alloc(), callInfo.getArg(0), clasp);
    current->add(guardToClass);
    current->push(guardToClass);
  }

  callInfo.setImplicitlyUsedUnchecked();
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineGetNextEntryForIterator(
    CallInfo& callInfo, MGetNextEntryForIterator::Mode mode) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 2);

  MDefinition* iterArg = callInfo.getArg(0);
  MDefinition* resultArg = callInfo.getArg(1);

  // Self-hosted code has already validated |iterArg| is a (possibly boxed)
  // Map- or SetIterator object.
  if (iterArg->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }

  if (resultArg->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }

  TemporaryTypeSet* resultTypes = resultArg->resultTypeSet();
  const JSClass* resultClasp =
      resultTypes ? resultTypes->getKnownClass(constraints()) : nullptr;
  if (resultClasp != &ArrayObject::class_) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MInstruction* next =
      MGetNextEntryForIterator::New(alloc(), iterArg, resultArg, mode);
  current->add(next);
  current->push(next);

  MOZ_TRY(resumeAfter(next));
  return InliningStatus_Inlined;
}

static bool IsArrayBufferObject(CompilerConstraintList* constraints,
                                MDefinition* def) {
  MOZ_ASSERT(def->type() == MIRType::Object);

  TemporaryTypeSet* types = def->resultTypeSet();
  if (!types) {
    return false;
  }

  return types->getKnownClass(constraints) == &ArrayBufferObject::class_;
}

IonBuilder::InliningResult IonBuilder::inlineArrayBufferByteLength(
    CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 1);

  MDefinition* objArg = callInfo.getArg(0);
  if (objArg->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }
  if (getInlineReturnType() != MIRType::Int32) {
    return InliningStatus_NotInlined;
  }

  MInstruction* ins = addArrayBufferByteLength(objArg);
  current->push(ins);

  callInfo.setImplicitlyUsedUnchecked();
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult
IonBuilder::inlinePossiblyWrappedArrayBufferByteLength(CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 1);

  MDefinition* objArg = callInfo.getArg(0);
  if (objArg->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }
  if (getInlineReturnType() != MIRType::Int32) {
    return InliningStatus_NotInlined;
  }

  if (!IsArrayBufferObject(constraints(), objArg)) {
    return InliningStatus_NotInlined;
  }

  MInstruction* ins = addArrayBufferByteLength(objArg);
  current->push(ins);

  callInfo.setImplicitlyUsedUnchecked();
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineTypedArray(CallInfo& callInfo,
                                                        Native native) {
  if (!callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  if (getInlineReturnType() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }
  if (callInfo.argc() == 0 || callInfo.argc() > 3) {
    return InliningStatus_NotInlined;
  }

  JSObject* templateObject = inspector->getTemplateObjectForNative(pc, native);
  if (!templateObject) {
    return InliningStatus_NotInlined;
  }
  MOZ_ASSERT(templateObject->is<TypedArrayObject>());
  TypedArrayObject* obj = &templateObject->as<TypedArrayObject>();

  // Do not optimize when we see a template object with a singleton type,
  // since it hits at most once.
  if (templateObject->isSingleton()) {
    return InliningStatus_NotInlined;
  }

  MDefinition* arg = callInfo.getArg(0);
  MInstruction* ins;
  if (arg->type() == MIRType::Int32) {
    if (!arg->isConstant()) {
      ins = MNewTypedArrayDynamicLength::New(
          alloc(), constraints(), templateObject,
          templateObject->group()->initialHeap(constraints()), arg);
    } else {
      // Negative lengths must throw a RangeError.  (We don't track that this
      // might have previously thrown, when determining whether to inline, so we
      // have to deal with this error case when inlining.)
      int32_t providedLen = arg->maybeConstantValue()->toInt32();
      if (providedLen <= 0) {
        return InliningStatus_NotInlined;
      }

      uint32_t len = AssertedCast<uint32_t>(providedLen);
      if (obj->length() != len) {
        return InliningStatus_NotInlined;
      }

      MConstant* templateConst =
          MConstant::NewConstraintlessObject(alloc(), obj);
      current->add(templateConst);
      ins = MNewTypedArray::New(alloc(), constraints(), templateConst,
                                obj->group()->initialHeap(constraints()));
    }
  } else if (arg->type() == MIRType::Object) {
    TemporaryTypeSet* types = arg->resultTypeSet();
    if (!types) {
      return InliningStatus_NotInlined;
    }

    // Don't inline if the argument might be a wrapper.
    if (types->forAllClasses(constraints(), IsProxyClass) !=
        TemporaryTypeSet::ForAllResult::ALL_FALSE) {
      return InliningStatus_NotInlined;
    }

    // Don't inline if we saw mixed use of (Shared)ArrayBuffers and other
    // objects.
    auto IsArrayBufferMaybeSharedClass = [](const JSClass* clasp) {
      return clasp == &ArrayBufferObject::class_ ||
             clasp == &SharedArrayBufferObject::class_;
    };
    switch (
        types->forAllClasses(constraints(), IsArrayBufferMaybeSharedClass)) {
      case TemporaryTypeSet::ForAllResult::ALL_FALSE:
        ins = MNewTypedArrayFromArray::New(
            alloc(), constraints(), templateObject,
            templateObject->group()->initialHeap(constraints()), arg);
        break;
      case TemporaryTypeSet::ForAllResult::ALL_TRUE:
        MDefinition* byteOffset;
        if (callInfo.argc() > 1) {
          byteOffset = callInfo.getArg(1);
        } else {
          byteOffset = constant(UndefinedValue());
        }

        MDefinition* length;
        if (callInfo.argc() > 2) {
          length = callInfo.getArg(2);
        } else {
          length = constant(UndefinedValue());
        }

        ins = MNewTypedArrayFromArrayBuffer::New(
            alloc(), constraints(), templateObject,
            templateObject->group()->initialHeap(constraints()), arg,
            byteOffset, length);
        break;
      case TemporaryTypeSet::ForAllResult::EMPTY:
      case TemporaryTypeSet::ForAllResult::MIXED:
        return InliningStatus_NotInlined;
    }
  } else {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();
  current->add(ins);
  current->push(ins);
  MOZ_TRY(resumeAfter(ins));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineIsTypedArrayConstructor(
    CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 1);

  if (getInlineReturnType() != MIRType::Boolean) {
    return InliningStatus_NotInlined;
  }
  if (callInfo.getArg(0)->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }

  // Try inlining with a constant if the argument is definitely a TypedArray
  // constructor.
  TemporaryTypeSet* types = callInfo.getArg(0)->resultTypeSet();
  if (!types || types->unknownObject() || types->getObjectCount() == 0) {
    return InliningStatus_NotInlined;
  }
  for (unsigned i = 0; i < types->getObjectCount(); i++) {
    JSObject* singleton = types->getSingleton(i);
    if (!singleton || !IsTypedArrayConstructor(singleton)) {
      return InliningStatus_NotInlined;
    }
  }

  callInfo.setImplicitlyUsedUnchecked();

  pushConstant(BooleanValue(true));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineIsTypedArrayHelper(
    CallInfo& callInfo, WrappingBehavior wrappingBehavior) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 1);

  if (callInfo.getArg(0)->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }
  if (getInlineReturnType() != MIRType::Boolean) {
    return InliningStatus_NotInlined;
  }

  // The test is elaborate: in-line only if there is exact
  // information.

  TemporaryTypeSet* types = callInfo.getArg(0)->resultTypeSet();
  if (!types) {
    return InliningStatus_NotInlined;
  }

  // Wrapped typed arrays won't appear to be typed arrays per a
  // |forAllClasses| query.  If wrapped typed arrays are to be considered
  // typed arrays, a negative answer is not conclusive.
  auto isPossiblyWrapped = [this, wrappingBehavior, types]() {
    if (wrappingBehavior != AllowWrappedTypedArrays) {
      return false;
    }

    switch (types->forAllClasses(constraints(), IsProxyClass)) {
      case TemporaryTypeSet::ForAllResult::ALL_FALSE:
      case TemporaryTypeSet::ForAllResult::EMPTY:
        break;
      case TemporaryTypeSet::ForAllResult::ALL_TRUE:
      case TemporaryTypeSet::ForAllResult::MIXED:
        return true;
    }
    return false;
  };

  bool result = false;
  bool isConstant = true;
  bool possiblyWrapped = false;
  switch (types->forAllClasses(constraints(), IsTypedArrayClass)) {
    case TemporaryTypeSet::ForAllResult::ALL_FALSE:
      if (isPossiblyWrapped()) {
        // Don't inline if we never saw typed arrays, but always only proxies.
        return InliningStatus_NotInlined;
      }

      [[fallthrough]];

    case TemporaryTypeSet::ForAllResult::EMPTY:
      result = false;
      break;

    case TemporaryTypeSet::ForAllResult::ALL_TRUE:
      result = true;
      break;

    case TemporaryTypeSet::ForAllResult::MIXED:
      isConstant = false;
      possiblyWrapped = isPossiblyWrapped();
      break;
  }

  if (isConstant) {
    pushConstant(BooleanValue(result));
  } else {
    auto* ins =
        MIsTypedArray::New(alloc(), callInfo.getArg(0), possiblyWrapped);
    current->add(ins);
    current->push(ins);

    if (possiblyWrapped) {
      MOZ_TRY(resumeAfter(ins));
    }
  }

  callInfo.setImplicitlyUsedUnchecked();
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineIsTypedArray(CallInfo& callInfo) {
  return inlineIsTypedArrayHelper(callInfo, RejectWrappedTypedArrays);
}

IonBuilder::InliningResult IonBuilder::inlineIsPossiblyWrappedTypedArray(
    CallInfo& callInfo) {
  return inlineIsTypedArrayHelper(callInfo, AllowWrappedTypedArrays);
}

static bool IsTypedArrayObject(CompilerConstraintList* constraints,
                               MDefinition* def) {
  MOZ_ASSERT(def->type() == MIRType::Object);

  TemporaryTypeSet* types = def->resultTypeSet();
  if (!types) {
    return false;
  }

  return types->forAllClasses(constraints, IsTypedArrayClass) ==
         TemporaryTypeSet::ForAllResult::ALL_TRUE;
}

IonBuilder::InliningResult IonBuilder::inlinePossiblyWrappedTypedArrayLength(
    CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 1);
  if (callInfo.getArg(0)->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }
  if (getInlineReturnType() != MIRType::Int32) {
    return InliningStatus_NotInlined;
  }

  if (!IsTypedArrayObject(constraints(), callInfo.getArg(0))) {
    return InliningStatus_NotInlined;
  }

  MInstruction* length = addTypedArrayLength(callInfo.getArg(0));
  current->push(length);

  callInfo.setImplicitlyUsedUnchecked();
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineTypedArrayLength(
    CallInfo& callInfo) {
  return inlinePossiblyWrappedTypedArrayLength(callInfo);
}

IonBuilder::InliningResult IonBuilder::inlineTypedArrayByteOffset(
    CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 1);
  if (callInfo.getArg(0)->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }
  if (getInlineReturnType() != MIRType::Int32) {
    return InliningStatus_NotInlined;
  }

  if (!IsTypedArrayObject(constraints(), callInfo.getArg(0))) {
    return InliningStatus_NotInlined;
  }

  MInstruction* byteOffset = addTypedArrayByteOffset(callInfo.getArg(0));
  current->push(byteOffset);

  callInfo.setImplicitlyUsedUnchecked();
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineTypedArrayElementShift(
    CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 1);
  if (callInfo.getArg(0)->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }
  if (getInlineReturnType() != MIRType::Int32) {
    return InliningStatus_NotInlined;
  }

  if (!IsTypedArrayObject(constraints(), callInfo.getArg(0))) {
    return InliningStatus_NotInlined;
  }

  auto* ins = MTypedArrayElementShift::New(alloc(), callInfo.getArg(0));
  current->add(ins);
  current->push(ins);

  callInfo.setImplicitlyUsedUnchecked();
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineUnsafeSetReservedSlot(
    CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 3);

  if (getInlineReturnType() != MIRType::Undefined) {
    return InliningStatus_NotInlined;
  }

  MDefinition* obj = callInfo.getArg(0);
  if (obj->type() != MIRType::Object && obj->type() != MIRType::Value) {
    return InliningStatus_NotInlined;
  }

  MDefinition* arg = callInfo.getArg(1);
  if (arg->type() != MIRType::Int32) {
    return InliningStatus_NotInlined;
  }

  // Don't inline if we don't have a constant slot.
  if (!arg->isConstant()) {
    return InliningStatus_NotInlined;
  }
  uint32_t slot = uint32_t(arg->toConstant()->toInt32());

  // Don't inline if it's not a fixed slot.
  if (slot >= NativeObject::MAX_FIXED_SLOTS) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MStoreFixedSlot* store =
      MStoreFixedSlot::NewBarriered(alloc(), obj, slot, callInfo.getArg(2));
  current->add(store);
  current->push(store);

  if (needsPostBarrier(callInfo.getArg(2))) {
    current->add(MPostWriteBarrier::New(alloc(), obj, callInfo.getArg(2)));
  }

  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineUnsafeGetReservedSlot(
    CallInfo& callInfo, MIRType knownValueType) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 2);

  MDefinition* obj = callInfo.getArg(0);
  if (obj->type() != MIRType::Object && obj->type() != MIRType::Value) {
    return InliningStatus_NotInlined;
  }

  MDefinition* arg = callInfo.getArg(1);
  if (arg->type() != MIRType::Int32) {
    return InliningStatus_NotInlined;
  }

  // Don't inline if we don't have a constant slot.
  if (!arg->isConstant()) {
    return InliningStatus_NotInlined;
  }
  uint32_t slot = uint32_t(arg->toConstant()->toInt32());

  // Don't inline if it's not a fixed slot.
  if (slot >= NativeObject::MAX_FIXED_SLOTS) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MLoadFixedSlot* load = MLoadFixedSlot::New(alloc(), obj, slot);
  current->add(load);
  current->push(load);
  if (knownValueType != MIRType::Value) {
    // We know what type we have in this slot.  Assert that this is in fact
    // what we've seen coming from this slot in the past, then tell the
    // MLoadFixedSlot about its result type.  That will make us do an
    // infallible unbox as part of the slot load and then we'll barrier on
    // the unbox result.  That way the type barrier code won't end up doing
    // MIRType checks and conditional unboxing.
    MOZ_ASSERT_IF(!getInlineReturnTypeSet()->empty(),
                  getInlineReturnType() == knownValueType);
    load->setResultType(knownValueType);
  }

  // We don't track reserved slot types, so always emit a barrier.
  MOZ_TRY(
      pushTypeBarrier(load, getInlineReturnTypeSet(), BarrierKind::TypeSet));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineIsCallable(CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 1);

  if (getInlineReturnType() != MIRType::Boolean) {
    return InliningStatus_NotInlined;
  }

  MDefinition* arg = callInfo.getArg(0);

  // Try inlining with constant true/false: only objects may be callable at
  // all, and if we know the class check if it is callable.
  bool isCallableKnown = false;
  bool isCallableConstant;
  if (arg->type() == MIRType::Object) {
    TemporaryTypeSet* types = arg->resultTypeSet();
    const JSClass* clasp =
        types ? types->getKnownClass(constraints()) : nullptr;
    if (clasp && !clasp->isProxy()) {
      isCallableKnown = true;
      isCallableConstant = clasp->nonProxyCallable();
    }
  } else if (!arg->mightBeType(MIRType::Object)) {
    // Primitive (including undefined and null).
    isCallableKnown = true;
    isCallableConstant = false;
  } else if (arg->type() != MIRType::Value) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  if (isCallableKnown) {
    MConstant* constant =
        MConstant::New(alloc(), BooleanValue(isCallableConstant));
    current->add(constant);
    current->push(constant);
    return InliningStatus_Inlined;
  }

  MIsCallable* isCallable = MIsCallable::New(alloc(), arg);
  current->add(isCallable);
  current->push(isCallable);

  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineIsConstructor(CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 1);

  if (getInlineReturnType() != MIRType::Boolean) {
    return InliningStatus_NotInlined;
  }
  if (callInfo.getArg(0)->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MIsConstructor* ins = MIsConstructor::New(alloc(), callInfo.getArg(0));
  current->add(ins);
  current->push(ins);

  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineIsObject(CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 1);

  if (getInlineReturnType() != MIRType::Boolean) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();
  if (callInfo.getArg(0)->type() == MIRType::Object) {
    pushConstant(BooleanValue(true));
  } else {
    MIsObject* isObject = MIsObject::New(alloc(), callInfo.getArg(0));
    current->add(isObject);
    current->push(isObject);
  }
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineToObject(CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 1);

  if (getInlineReturnType() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }

  MDefinition* object = callInfo.getArg(0);
  if (object->type() != MIRType::Object && object->type() != MIRType::Value) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  // If we know the input type is an object, nop ToObject.
  if (object->type() == MIRType::Object) {
    current->push(object);
  } else {
    auto* ins = MToObject::New(alloc(), object);
    current->add(ins);
    current->push(ins);

    MOZ_TRY(
        pushTypeBarrier(ins, getInlineReturnTypeSet(), BarrierKind::TypeSet));
  }

  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineIsCrossRealmArrayConstructor(
    CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 1);

  if (getInlineReturnType() != MIRType::Boolean) {
    return InliningStatus_NotInlined;
  }
  MDefinition* arg = callInfo.getArg(0);
  if (arg->type() != MIRType::Object) {
    return InliningStatus_NotInlined;
  }

  TemporaryTypeSet* types = arg->resultTypeSet();
  Realm* realm = types->getKnownRealm(constraints());
  if (!realm || realm != script()->realm()) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  // Inline only if argument is absolutely *not* a wrapper or a cross-realm
  // object.
  pushConstant(BooleanValue(false));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineToInteger(CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 1);

  MDefinition* input = callInfo.getArg(0);

  // Only optimize cases where |input| contains only number, null, undefined, or
  // boolean.
  if (input->mightBeType(MIRType::Object) ||
      input->mightBeType(MIRType::String) ||
      input->mightBeType(MIRType::Symbol) ||
      input->mightBeType(MIRType::BigInt) || input->mightBeMagicType()) {
    return InliningStatus_NotInlined;
  }

  MOZ_ASSERT(input->type() == MIRType::Value ||
             input->type() == MIRType::Null ||
             input->type() == MIRType::Undefined ||
             input->type() == MIRType::Boolean || IsNumberType(input->type()));

  // Only optimize cases where the output is int32 or double.
  MIRType returnType = getInlineReturnType();
  if (returnType != MIRType::Int32 && returnType != MIRType::Double) {
    return InliningStatus_NotInlined;
  }

  if (returnType == MIRType::Int32) {
    auto* toInt32 = MToIntegerInt32::New(alloc(), input);
    current->add(toInt32);
    current->push(toInt32);
  } else {
    MInstruction* ins;
    if (MNearbyInt::HasAssemblerSupport(RoundingMode::TowardsZero)) {
      ins = MNearbyInt::New(alloc(), input, MIRType::Double,
                            RoundingMode::TowardsZero);
    } else {
      ins = MMathFunction::New(alloc(), input, MMathFunction::Trunc);
    }
    current->add(ins);

    auto* nanToZero = MNaNToZero::New(alloc(), ins);
    current->add(nanToZero);
    current->push(nanToZero);
  }

  callInfo.setImplicitlyUsedUnchecked();
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineToString(CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 1);

  if (getInlineReturnType() != MIRType::String) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();
  MToString* toString = MToString::New(
      alloc(), callInfo.getArg(0), MToString::SideEffectHandling::Supported);
  current->add(toString);
  current->push(toString);
  if (toString->isEffectful()) {
    MOZ_TRY(resumeAfter(toString));
  }
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineBailout(CallInfo& callInfo) {
  callInfo.setImplicitlyUsedUnchecked();

  current->add(MBail::New(alloc()));

  MConstant* undefined = MConstant::New(alloc(), UndefinedValue());
  current->add(undefined);
  current->push(undefined);
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineAssertFloat32(CallInfo& callInfo) {
  if (callInfo.argc() != 2) {
    return InliningStatus_NotInlined;
  }

  MDefinition* secondArg = callInfo.getArg(1);

  MOZ_ASSERT(secondArg->type() == MIRType::Boolean);
  MOZ_ASSERT(secondArg->isConstant());

  bool mustBeFloat32 = secondArg->toConstant()->toBoolean();
  current->add(MAssertFloat32::New(alloc(), callInfo.getArg(0), mustBeFloat32));

  MConstant* undefined = MConstant::New(alloc(), UndefinedValue());
  current->add(undefined);
  current->push(undefined);
  callInfo.setImplicitlyUsedUnchecked();
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineAssertRecoveredOnBailout(
    CallInfo& callInfo) {
  if (callInfo.argc() != 2) {
    return InliningStatus_NotInlined;
  }

  // Don't assert for recovered instructions when recovering is disabled.
  if (JitOptions.disableRecoverIns) {
    return InliningStatus_NotInlined;
  }

  if (JitOptions.checkRangeAnalysis) {
    // If we are checking the range of all instructions, then the guards
    // inserted by Range Analysis prevent the use of recover
    // instruction. Thus, we just disable these checks.
    current->push(constant(UndefinedValue()));
    callInfo.setImplicitlyUsedUnchecked();
    return InliningStatus_Inlined;
  }

  MDefinition* secondArg = callInfo.getArg(1);

  MOZ_ASSERT(secondArg->type() == MIRType::Boolean);
  MOZ_ASSERT(secondArg->isConstant());

  bool mustBeRecovered = secondArg->toConstant()->toBoolean();
  MAssertRecoveredOnBailout* assert = MAssertRecoveredOnBailout::New(
      alloc(), callInfo.getArg(0), mustBeRecovered);
  current->add(assert);
  current->push(assert);

  // Create an instruction sequence which implies that the argument of the
  // assertRecoveredOnBailout function would be encoded at least in one
  // Snapshot.
  MNop* nop = MNop::New(alloc());
  current->add(nop);
  MOZ_TRY(resumeAfter(nop));
  current->add(MEncodeSnapshot::New(alloc()));

  current->pop();
  current->push(constant(UndefinedValue()));
  callInfo.setImplicitlyUsedUnchecked();
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineAtomicsCompareExchange(
    CallInfo& callInfo) {
  if (callInfo.argc() != 4 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  // These guards are desirable here and in subsequent atomics to
  // avoid bad bailouts with MTruncateToInt32, see
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1141986#c20.
  MDefinition* oldval = callInfo.getArg(2);
  if (oldval->mightBeType(MIRType::Object) ||
      oldval->mightBeType(MIRType::Symbol) ||
      oldval->mightBeType(MIRType::BigInt)) {
    return InliningStatus_NotInlined;
  }

  MDefinition* newval = callInfo.getArg(3);
  if (newval->mightBeType(MIRType::Object) ||
      newval->mightBeType(MIRType::Symbol) ||
      newval->mightBeType(MIRType::BigInt)) {
    return InliningStatus_NotInlined;
  }

  Scalar::Type arrayType;
  bool requiresCheck = false;
  if (!atomicsMeetsPreconditions(callInfo, &arrayType, &requiresCheck)) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MInstruction* elements;
  MDefinition* index;
  atomicsCheckBounds(callInfo, &elements, &index);

  if (requiresCheck) {
    addSharedTypedArrayGuard(callInfo.getArg(0));
  }

  MCompareExchangeTypedArrayElement* cas =
      MCompareExchangeTypedArrayElement::New(alloc(), elements, index,
                                             arrayType, oldval, newval);
  cas->setResultType(getInlineReturnType());
  current->add(cas);
  current->push(cas);

  MOZ_TRY(resumeAfter(cas));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineAtomicsExchange(
    CallInfo& callInfo) {
  if (callInfo.argc() != 3 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  MDefinition* value = callInfo.getArg(2);
  if (value->mightBeType(MIRType::Object) ||
      value->mightBeType(MIRType::Symbol) ||
      value->mightBeType(MIRType::BigInt)) {
    return InliningStatus_NotInlined;
  }

  Scalar::Type arrayType;
  bool requiresCheck = false;
  if (!atomicsMeetsPreconditions(callInfo, &arrayType, &requiresCheck)) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MInstruction* elements;
  MDefinition* index;
  atomicsCheckBounds(callInfo, &elements, &index);

  if (requiresCheck) {
    addSharedTypedArrayGuard(callInfo.getArg(0));
  }

  MInstruction* exchange = MAtomicExchangeTypedArrayElement::New(
      alloc(), elements, index, value, arrayType);
  exchange->setResultType(getInlineReturnType());
  current->add(exchange);
  current->push(exchange);

  MOZ_TRY(resumeAfter(exchange));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineAtomicsLoad(CallInfo& callInfo) {
  if (callInfo.argc() != 2 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  Scalar::Type arrayType;
  bool requiresCheck = false;
  if (!atomicsMeetsPreconditions(callInfo, &arrayType, &requiresCheck)) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MInstruction* elements;
  MDefinition* index;
  atomicsCheckBounds(callInfo, &elements, &index);

  if (requiresCheck) {
    addSharedTypedArrayGuard(callInfo.getArg(0));
  }

  MLoadUnboxedScalar* load = MLoadUnboxedScalar::New(
      alloc(), elements, index, arrayType, DoesRequireMemoryBarrier);
  load->setResultType(getInlineReturnType());
  current->add(load);
  current->push(load);

  // Loads are considered effectful (they execute a memory barrier).
  MOZ_TRY(resumeAfter(load));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineAtomicsStore(CallInfo& callInfo) {
  if (callInfo.argc() != 3 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  // Atomics.store() is annoying because it returns the result of converting
  // the value by ToInteger(), not the input value, nor the result of
  // converting the value by ToInt32().  It is especially annoying because
  // almost nobody uses the result value.
  //
  // As an expedient compromise, therefore, we inline only if the result is
  // obviously unused or if the argument is already Int32 and thus requires no
  // conversion.

  MDefinition* value = callInfo.getArg(2);
  if (!BytecodeIsPopped(pc) && value->type() != MIRType::Int32) {
    return InliningStatus_NotInlined;
  }

  if (value->mightBeType(MIRType::Object) ||
      value->mightBeType(MIRType::Symbol) ||
      value->mightBeType(MIRType::BigInt)) {
    return InliningStatus_NotInlined;
  }

  Scalar::Type arrayType;
  bool requiresCheck = false;
  if (!atomicsMeetsPreconditions(callInfo, &arrayType, &requiresCheck,
                                 DontCheckAtomicResult)) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MInstruction* elements;
  MDefinition* index;
  atomicsCheckBounds(callInfo, &elements, &index);

  if (requiresCheck) {
    addSharedTypedArrayGuard(callInfo.getArg(0));
  }

  MDefinition* toWrite = value;
  if (toWrite->type() != MIRType::Int32) {
    toWrite = MTruncateToInt32::New(alloc(), toWrite);
    current->add(toWrite->toInstruction());
  }
  MStoreUnboxedScalar* store = MStoreUnboxedScalar::New(
      alloc(), elements, index, toWrite, arrayType,
      MStoreUnboxedScalar::TruncateInput, DoesRequireMemoryBarrier);
  current->add(store);
  current->push(value);  // Either Int32 or not used; in either case correct

  MOZ_TRY(resumeAfter(store));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineAtomicsBinop(
    CallInfo& callInfo, InlinableNative target) {
  if (callInfo.argc() != 3 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  MDefinition* value = callInfo.getArg(2);
  if (value->mightBeType(MIRType::Object) ||
      value->mightBeType(MIRType::Symbol) ||
      value->mightBeType(MIRType::BigInt)) {
    return InliningStatus_NotInlined;
  }

  Scalar::Type arrayType;
  bool requiresCheck = false;
  if (!atomicsMeetsPreconditions(callInfo, &arrayType, &requiresCheck)) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  if (requiresCheck) {
    addSharedTypedArrayGuard(callInfo.getArg(0));
  }

  MInstruction* elements;
  MDefinition* index;
  atomicsCheckBounds(callInfo, &elements, &index);

  AtomicOp k = AtomicFetchAddOp;
  switch (target) {
    case InlinableNative::AtomicsAdd:
      k = AtomicFetchAddOp;
      break;
    case InlinableNative::AtomicsSub:
      k = AtomicFetchSubOp;
      break;
    case InlinableNative::AtomicsAnd:
      k = AtomicFetchAndOp;
      break;
    case InlinableNative::AtomicsOr:
      k = AtomicFetchOrOp;
      break;
    case InlinableNative::AtomicsXor:
      k = AtomicFetchXorOp;
      break;
    default:
      MOZ_CRASH("Bad atomic operation");
  }

  MAtomicTypedArrayElementBinop* binop = MAtomicTypedArrayElementBinop::New(
      alloc(), k, elements, index, arrayType, value);
  binop->setResultType(getInlineReturnType());
  current->add(binop);
  current->push(binop);

  MOZ_TRY(resumeAfter(binop));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineAtomicsIsLockFree(
    CallInfo& callInfo) {
  if (callInfo.argc() != 1 || callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  MAtomicIsLockFree* ilf = MAtomicIsLockFree::New(alloc(), callInfo.getArg(0));
  current->add(ilf);
  current->push(ilf);

  return InliningStatus_Inlined;
}

bool IonBuilder::atomicsMeetsPreconditions(CallInfo& callInfo,
                                           Scalar::Type* arrayType,
                                           bool* requiresTagCheck,
                                           AtomicCheckResult checkResult) {
  if (!JitSupportsAtomics()) {
    return false;
  }

  if (callInfo.getArg(0)->type() != MIRType::Object) {
    return false;
  }

  if (callInfo.getArg(1)->type() != MIRType::Int32) {
    return false;
  }

  // Ensure that the first argument is a TypedArray that maps shared
  // memory.
  //
  // Then check both that the element type is something we can
  // optimize and that the return type is suitable for that element
  // type.

  TemporaryTypeSet* arg0Types = callInfo.getArg(0)->resultTypeSet();
  if (!arg0Types) {
    return false;
  }

  TemporaryTypeSet::TypedArraySharedness sharedness =
      TemporaryTypeSet::UnknownSharedness;
  *arrayType = arg0Types->getTypedArrayType(constraints(), &sharedness);
  *requiresTagCheck = sharedness != TemporaryTypeSet::KnownShared;
  switch (*arrayType) {
    case Scalar::Int8:
    case Scalar::Uint8:
    case Scalar::Int16:
    case Scalar::Uint16:
    case Scalar::Int32:
      return checkResult == DontCheckAtomicResult ||
             getInlineReturnType() == MIRType::Int32;
    case Scalar::Uint32:
      // Bug 1077305: it would be attractive to allow inlining even
      // if the inline return type is Int32, which it will frequently
      // be.
      return checkResult == DontCheckAtomicResult ||
             getInlineReturnType() == MIRType::Double;
    default:
      // Excludes floating types and Uint8Clamped.
      return false;
  }
}

void IonBuilder::atomicsCheckBounds(CallInfo& callInfo, MInstruction** elements,
                                    MDefinition** index) {
  // Perform bounds checking and extract the elements vector.
  MDefinition* obj = callInfo.getArg(0);
  MInstruction* length = nullptr;
  *index = callInfo.getArg(1);
  *elements = nullptr;
  addTypedArrayLengthAndData(obj, DoBoundsCheck, index, &length, elements);
}

IonBuilder::InliningResult IonBuilder::inlineIsConstructing(
    CallInfo& callInfo) {
  MOZ_ASSERT(!callInfo.constructing());
  MOZ_ASSERT(callInfo.argc() == 0);
  MOZ_ASSERT(script()->isFunction(),
             "isConstructing() should only be called in function scripts");

  if (getInlineReturnType() != MIRType::Boolean) {
    return InliningStatus_NotInlined;
  }

  callInfo.setImplicitlyUsedUnchecked();

  if (inliningDepth_ == 0) {
    MInstruction* ins = MIsConstructing::New(alloc());
    current->add(ins);
    current->push(ins);
    return InliningStatus_Inlined;
  }

  bool constructing = inlineCallInfo_->constructing();
  pushConstant(BooleanValue(constructing));
  return InliningStatus_Inlined;
}

IonBuilder::InliningResult IonBuilder::inlineWasmCall(CallInfo& callInfo,
                                                      JSFunction* target) {
  MOZ_ASSERT(target->isWasmWithJitEntry());

  // Don't inline wasm constructors.
  if (callInfo.constructing()) {
    return InliningStatus_NotInlined;
  }

  if (target->realm() != script()->realm()) {
    return InliningStatus_NotInlined;
  }

  wasm::Instance& inst = wasm::ExportedFunctionToInstance(target);
  uint32_t funcIndex = inst.code().getFuncIndex(target);

  auto bestTier = inst.code().bestTier();
  const wasm::FuncExport& funcExport =
      inst.metadata(bestTier).lookupFuncExport(funcIndex);
  const wasm::FuncType& sig = funcExport.funcType();

  // Check that the function doesn't take or return non-compatible JS
  // argument types before adding nodes to the MIR graph, otherwise they'd be
  // dead code.
  if (sig.hasI64ArgOrRet() ||
      sig.temporarilyUnsupportedReftypeForInlineEntry() ||
      !JitOptions.enableWasmIonFastCalls) {
    return InliningStatus_NotInlined;
  }

  // If there are too many arguments, don't inline (we won't be able to store
  // the arguments in the LIR node).
  static constexpr size_t MaxNumInlinedArgs = 8;
  static_assert(MaxNumInlinedArgs <= MaxNumLInstructionOperands,
                "inlined arguments can all be LIR operands");
  if (sig.args().length() > MaxNumInlinedArgs) {
    return InliningStatus_NotInlined;
  }

  // If there are too many results, don't inline as we don't currently
  // add MWasmStackResults.
  if (sig.results().length() > wasm::MaxResultsForJitInlineCall) {
    return InliningStatus_NotInlined;
  }

  auto* call = MIonToWasmCall::New(alloc(), inst.object(), funcExport);
  if (!call) {
    return abort(AbortReason::Alloc);
  }

  // An invariant in this loop is that any type conversion operation that has
  // externally visible effects, such as invoking valueOf on an object argument,
  // must bailout so that we don't have to worry about replaying effects during
  // argument conversion.
  Maybe<MDefinition*> undefined;
  for (size_t i = 0; i < sig.args().length(); i++) {
    if (!alloc().ensureBallast()) {
      return abort(AbortReason::Alloc);
    }

    // Add undefined if an argument is missing.
    if (i >= callInfo.argc() && !undefined) {
      undefined.emplace(constant(UndefinedValue()));
    }

    MDefinition* arg = i >= callInfo.argc() ? *undefined : callInfo.getArg(i);

    MInstruction* conversion = nullptr;
    switch (sig.args()[i].kind()) {
      case wasm::ValType::I32:
        conversion = MTruncateToInt32::New(alloc(), arg);
        break;
      case wasm::ValType::F32:
        conversion = MToFloat32::New(alloc(), arg);
        break;
      case wasm::ValType::F64:
        conversion = MToDouble::New(alloc(), arg);
        break;
      case wasm::ValType::Ref:
        switch (sig.args()[i].refTypeKind()) {
          case wasm::RefType::Any:
            // Transform the JS representation into an AnyRef representation.
            // The resulting type is MIRType::RefOrNull.  These cases are all
            // effect-free.
            switch (arg->type()) {
              case MIRType::Object:
              case MIRType::ObjectOrNull:
                conversion = MWasmAnyRefFromJSObject::New(alloc(), arg);
                break;
              case MIRType::Null:
                conversion = MWasmNullConstant::New(alloc());
                break;
              default:
                conversion = MWasmBoxValue::New(alloc(), arg);
                break;
            }
            break;
          default:
            MOZ_CRASH("impossible per above check");
        }
        break;
      case wasm::ValType::I64:
        MOZ_CRASH("impossible per above check");
    }

    current->add(conversion);
    call->initArg(i, conversion);
  }

  current->push(call);
  current->add(call);

  MOZ_TRY(resumeAfter(call));

  callInfo.setImplicitlyUsedUnchecked();

  return InliningStatus_Inlined;
}

#define ADD_NATIVE(native)                                              \
  const JSJitInfo JitInfo_##native{{nullptr},                           \
                                   {uint16_t(InlinableNative::native)}, \
                                   {0},                                 \
                                   JSJitInfo::InlinableNative};
INLINABLE_NATIVE_LIST(ADD_NATIVE)
#undef ADD_NATIVE

}  // namespace jit
}  // namespace js
