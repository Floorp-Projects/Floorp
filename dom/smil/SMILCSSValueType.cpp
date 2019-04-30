/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of a value for a SMIL-animated CSS property */

#include "SMILCSSValueType.h"

#include "nsComputedDOMStyle.h"
#include "nsColor.h"
#include "nsCSSProps.h"
#include "nsCSSValue.h"
#include "nsDebug.h"
#include "nsPresContext.h"
#include "nsString.h"
#include "nsStyleUtil.h"
#include "mozilla/DeclarationBlock.h"
#include "mozilla/PresShell.h"
#include "mozilla/PresShellInlines.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/StyleAnimationValue.h"
#include "mozilla/ServoCSSParser.h"
#include "mozilla/ServoStyleSet.h"
#include "mozilla/SMILParserUtils.h"
#include "mozilla/SMILValue.h"
#include "mozilla/dom/BaseKeyframeTypesBinding.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"

using namespace mozilla::dom;

namespace mozilla {

typedef AutoTArray<RefPtr<RawServoAnimationValue>, 1> ServoAnimationValues;

/*static*/
SMILCSSValueType SMILCSSValueType::sSingleton;

struct ValueWrapper {
  ValueWrapper(nsCSSPropertyID aPropID, const AnimationValue& aValue)
      : mPropID(aPropID) {
    MOZ_ASSERT(!aValue.IsNull());
    mServoValues.AppendElement(aValue.mServo);
  }
  ValueWrapper(nsCSSPropertyID aPropID,
               const RefPtr<RawServoAnimationValue>& aValue)
      : mPropID(aPropID), mServoValues{(aValue)} {}
  ValueWrapper(nsCSSPropertyID aPropID, ServoAnimationValues&& aValues)
      : mPropID(aPropID), mServoValues{aValues} {}

  bool operator==(const ValueWrapper& aOther) const {
    if (mPropID != aOther.mPropID) {
      return false;
    }

    MOZ_ASSERT(!mServoValues.IsEmpty());
    size_t len = mServoValues.Length();
    if (len != aOther.mServoValues.Length()) {
      return false;
    }
    for (size_t i = 0; i < len; i++) {
      if (!Servo_AnimationValue_DeepEqual(mServoValues[i],
                                          aOther.mServoValues[i])) {
        return false;
      }
    }
    return true;
  }

  bool operator!=(const ValueWrapper& aOther) const {
    return !(*this == aOther);
  }

  nsCSSPropertyID mPropID;
  ServoAnimationValues mServoValues;
};

// Helper Methods
// --------------

// If one argument is null, this method updates it to point to "zero"
// for the other argument's Unit (if applicable; otherwise, we return false).
//
// If neither argument is null, this method simply returns true.
//
// If both arguments are null, this method returns false.
//
// |aZeroValueStorage| should be a reference to a
// RefPtr<RawServoAnimationValue>. This is used where we may need to allocate a
// new ServoAnimationValue to represent the appropriate zero value.
//
// Returns true on success, or otherwise.
static bool FinalizeServoAnimationValues(
    const RefPtr<RawServoAnimationValue>*& aValue1,
    const RefPtr<RawServoAnimationValue>*& aValue2,
    RefPtr<RawServoAnimationValue>& aZeroValueStorage) {
  if (!aValue1 && !aValue2) {
    return false;
  }

  // Are we missing either val? (If so, it's an implied 0 in other val's units)

  if (!aValue1) {
    aZeroValueStorage = Servo_AnimationValues_GetZeroValue(*aValue2).Consume();
    aValue1 = &aZeroValueStorage;
  } else if (!aValue2) {
    aZeroValueStorage = Servo_AnimationValues_GetZeroValue(*aValue1).Consume();
    aValue2 = &aZeroValueStorage;
  }
  return *aValue1 && *aValue2;
}

static ValueWrapper* ExtractValueWrapper(SMILValue& aValue) {
  return static_cast<ValueWrapper*>(aValue.mU.mPtr);
}

static const ValueWrapper* ExtractValueWrapper(const SMILValue& aValue) {
  return static_cast<const ValueWrapper*>(aValue.mU.mPtr);
}

// Class methods
// -------------
void SMILCSSValueType::Init(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.IsNull(), "Unexpected SMIL value type");

  aValue.mU.mPtr = nullptr;
  aValue.mType = this;
}

void SMILCSSValueType::Destroy(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.mType == this, "Unexpected SMIL value type");
  delete static_cast<ValueWrapper*>(aValue.mU.mPtr);
  aValue.mType = SMILNullType::Singleton();
}

nsresult SMILCSSValueType::Assign(SMILValue& aDest,
                                  const SMILValue& aSrc) const {
  MOZ_ASSERT(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aDest.mType == this, "Unexpected SMIL value type");
  const ValueWrapper* srcWrapper = ExtractValueWrapper(aSrc);
  ValueWrapper* destWrapper = ExtractValueWrapper(aDest);

  if (srcWrapper) {
    if (!destWrapper) {
      // barely-initialized dest -- need to alloc & copy
      aDest.mU.mPtr = new ValueWrapper(*srcWrapper);
    } else {
      // both already fully-initialized -- just copy straight across
      *destWrapper = *srcWrapper;
    }
  } else if (destWrapper) {
    // fully-initialized dest, barely-initialized src -- clear dest
    delete destWrapper;
    aDest.mU.mPtr = destWrapper = nullptr;
  }  // else, both are barely-initialized -- nothing to do.

  return NS_OK;
}

bool SMILCSSValueType::IsEqual(const SMILValue& aLeft,
                               const SMILValue& aRight) const {
  MOZ_ASSERT(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aLeft.mType == this, "Unexpected SMIL value");
  const ValueWrapper* leftWrapper = ExtractValueWrapper(aLeft);
  const ValueWrapper* rightWrapper = ExtractValueWrapper(aRight);

  if (leftWrapper) {
    if (rightWrapper) {
      // Both non-null
      NS_WARNING_ASSERTION(leftWrapper != rightWrapper,
                           "Two SMILValues with matching ValueWrapper ptr");
      return *leftWrapper == *rightWrapper;
    }
    // Left non-null, right null
    return false;
  }
  if (rightWrapper) {
    // Left null, right non-null
    return false;
  }
  // Both null
  return true;
}

static bool AddOrAccumulateForServo(SMILValue& aDest,
                                    const ValueWrapper* aValueToAddWrapper,
                                    ValueWrapper* aDestWrapper,
                                    CompositeOperation aCompositeOp,
                                    uint64_t aCount) {
  nsCSSPropertyID property =
      aValueToAddWrapper ? aValueToAddWrapper->mPropID : aDestWrapper->mPropID;
  size_t len = aValueToAddWrapper ? aValueToAddWrapper->mServoValues.Length()
                                  : aDestWrapper->mServoValues.Length();

  MOZ_ASSERT(!aValueToAddWrapper || !aDestWrapper ||
                 aValueToAddWrapper->mServoValues.Length() ==
                     aDestWrapper->mServoValues.Length(),
             "Both of values' length in the wrappers should be the same if "
             "both of them exist");

  for (size_t i = 0; i < len; i++) {
    const RefPtr<RawServoAnimationValue>* valueToAdd =
        aValueToAddWrapper ? &aValueToAddWrapper->mServoValues[i] : nullptr;
    const RefPtr<RawServoAnimationValue>* destValue =
        aDestWrapper ? &aDestWrapper->mServoValues[i] : nullptr;
    RefPtr<RawServoAnimationValue> zeroValueStorage;
    if (!FinalizeServoAnimationValues(valueToAdd, destValue,
                                      zeroValueStorage)) {
      return false;
    }

    // FinalizeServoAnimationValues may have updated destValue so we should make
    // sure the aDest and aDestWrapper outparams are up-to-date.
    if (aDestWrapper) {
      aDestWrapper->mServoValues[i] = *destValue;
    } else {
      // aDest may be a barely-initialized "zero" destination.
      aDest.mU.mPtr = aDestWrapper = new ValueWrapper(property, *destValue);
      aDestWrapper->mServoValues.SetLength(len);
    }

    RefPtr<RawServoAnimationValue> result;
    if (aCompositeOp == CompositeOperation::Add) {
      result = Servo_AnimationValues_Add(*destValue, *valueToAdd).Consume();
    } else {
      result = Servo_AnimationValues_Accumulate(*destValue, *valueToAdd, aCount)
                   .Consume();
    }

    if (!result) {
      return false;
    }
    aDestWrapper->mServoValues[i] = result;
  }

  return true;
}

static bool AddOrAccumulate(SMILValue& aDest, const SMILValue& aValueToAdd,
                            CompositeOperation aCompositeOp, uint64_t aCount) {
  MOZ_ASSERT(aValueToAdd.mType == aDest.mType,
             "Trying to add mismatching types");
  MOZ_ASSERT(aValueToAdd.mType == &SMILCSSValueType::sSingleton,
             "Unexpected SMIL value type");
  MOZ_ASSERT(aCompositeOp == CompositeOperation::Add ||
                 aCompositeOp == CompositeOperation::Accumulate,
             "Composite operation should be add or accumulate");
  MOZ_ASSERT(aCompositeOp != CompositeOperation::Add || aCount == 1,
             "Count should be 1 if composite operation is add");

  ValueWrapper* destWrapper = ExtractValueWrapper(aDest);
  const ValueWrapper* valueToAddWrapper = ExtractValueWrapper(aValueToAdd);

  // If both of the values are empty just fail. This can happen in rare cases
  // such as when the underlying animation produced an empty value.
  //
  // Technically, it doesn't matter what we return here since in either case it
  // will produce the same result: an empty value.
  if (!destWrapper && !valueToAddWrapper) {
    return false;
  }

  nsCSSPropertyID property =
      valueToAddWrapper ? valueToAddWrapper->mPropID : destWrapper->mPropID;
  // Special case: font-size-adjust and stroke-dasharray are explicitly
  // non-additive (even though StyleAnimationValue *could* support adding them)
  if (property == eCSSProperty_font_size_adjust ||
      property == eCSSProperty_stroke_dasharray) {
    return false;
  }
  // Skip font shorthand since it includes font-size-adjust.
  if (property == eCSSProperty_font) {
    return false;
  }

  return AddOrAccumulateForServo(aDest, valueToAddWrapper, destWrapper,
                                 aCompositeOp, aCount);
}

nsresult SMILCSSValueType::SandwichAdd(SMILValue& aDest,
                                       const SMILValue& aValueToAdd) const {
  return AddOrAccumulate(aDest, aValueToAdd, CompositeOperation::Add, 1)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

nsresult SMILCSSValueType::Add(SMILValue& aDest, const SMILValue& aValueToAdd,
                               uint32_t aCount) const {
  return AddOrAccumulate(aDest, aValueToAdd, CompositeOperation::Accumulate,
                         aCount)
             ? NS_OK
             : NS_ERROR_FAILURE;
}

static nsresult ComputeDistanceForServo(const ValueWrapper* aFromWrapper,
                                        const ValueWrapper& aToWrapper,
                                        double& aDistance) {
  size_t len = aToWrapper.mServoValues.Length();
  MOZ_ASSERT(!aFromWrapper || aFromWrapper->mServoValues.Length() == len,
             "From and to values length should be the same if "
             "The start value exists");

  double squareDistance = 0;

  for (size_t i = 0; i < len; i++) {
    const RefPtr<RawServoAnimationValue>* fromValue =
        aFromWrapper ? &aFromWrapper->mServoValues[0] : nullptr;
    const RefPtr<RawServoAnimationValue>* toValue = &aToWrapper.mServoValues[0];
    RefPtr<RawServoAnimationValue> zeroValueStorage;
    if (!FinalizeServoAnimationValues(fromValue, toValue, zeroValueStorage)) {
      return NS_ERROR_FAILURE;
    }

    double distance =
        Servo_AnimationValues_ComputeDistance(*fromValue, *toValue);
    if (distance < 0.0) {
      return NS_ERROR_FAILURE;
    }

    if (len == 1) {
      aDistance = distance;
      return NS_OK;
    }
    squareDistance += distance * distance;
  }

  aDistance = sqrt(squareDistance);

  return NS_OK;
}

nsresult SMILCSSValueType::ComputeDistance(const SMILValue& aFrom,
                                           const SMILValue& aTo,
                                           double& aDistance) const {
  MOZ_ASSERT(aFrom.mType == aTo.mType, "Trying to compare different types");
  MOZ_ASSERT(aFrom.mType == this, "Unexpected source type");

  const ValueWrapper* fromWrapper = ExtractValueWrapper(aFrom);
  const ValueWrapper* toWrapper = ExtractValueWrapper(aTo);
  MOZ_ASSERT(toWrapper, "expecting non-null endpoint");
  return ComputeDistanceForServo(fromWrapper, *toWrapper, aDistance);
}

static nsresult InterpolateForServo(const ValueWrapper* aStartWrapper,
                                    const ValueWrapper& aEndWrapper,
                                    double aUnitDistance, SMILValue& aResult) {
  // For discretely-animated properties Servo_AnimationValues_Interpolate will
  // perform the discrete animation (i.e. 50% flip) and return a success result.
  // However, SMIL has its own special discrete animation behavior that it uses
  // when keyTimes are specified, but we won't run that unless that this method
  // returns a failure to indicate that the property cannot be smoothly
  // interpolated, i.e. that we need to use a discrete calcMode.
  //
  // For shorthands, Servo_Property_IsDiscreteAnimatable will always return
  // false. That's fine since most shorthands (like 'font' and
  // 'text-decoration') include non-discrete components. If authors want to
  // treat all components as discrete then they should use calcMode="discrete".
  if (Servo_Property_IsDiscreteAnimatable(aEndWrapper.mPropID)) {
    return NS_ERROR_FAILURE;
  }

  ServoAnimationValues results;
  size_t len = aEndWrapper.mServoValues.Length();
  results.SetCapacity(len);
  MOZ_ASSERT(!aStartWrapper || aStartWrapper->mServoValues.Length() == len,
             "Start and end values length should be the same if "
             "the start value exists");
  for (size_t i = 0; i < len; i++) {
    const RefPtr<RawServoAnimationValue>* startValue =
        aStartWrapper ? &aStartWrapper->mServoValues[i] : nullptr;
    const RefPtr<RawServoAnimationValue>* endValue =
        &aEndWrapper.mServoValues[i];
    RefPtr<RawServoAnimationValue> zeroValueStorage;
    if (!FinalizeServoAnimationValues(startValue, endValue, zeroValueStorage)) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<RawServoAnimationValue> result =
        Servo_AnimationValues_Interpolate(*startValue, *endValue, aUnitDistance)
            .Consume();
    if (!result) {
      return NS_ERROR_FAILURE;
    }
    results.AppendElement(result);
  }
  aResult.mU.mPtr = new ValueWrapper(aEndWrapper.mPropID, std::move(results));

  return NS_OK;
}

nsresult SMILCSSValueType::Interpolate(const SMILValue& aStartVal,
                                       const SMILValue& aEndVal,
                                       double aUnitDistance,
                                       SMILValue& aResult) const {
  MOZ_ASSERT(aStartVal.mType == aEndVal.mType,
             "Trying to interpolate different types");
  MOZ_ASSERT(aStartVal.mType == this, "Unexpected types for interpolation");
  MOZ_ASSERT(aResult.mType == this, "Unexpected result type");
  MOZ_ASSERT(aUnitDistance >= 0.0 && aUnitDistance <= 1.0,
             "unit distance value out of bounds");
  MOZ_ASSERT(!aResult.mU.mPtr, "expecting barely-initialized outparam");

  const ValueWrapper* startWrapper = ExtractValueWrapper(aStartVal);
  const ValueWrapper* endWrapper = ExtractValueWrapper(aEndVal);
  MOZ_ASSERT(endWrapper, "expecting non-null endpoint");
  return InterpolateForServo(startWrapper, *endWrapper, aUnitDistance, aResult);
}

static ServoAnimationValues ValueFromStringHelper(nsCSSPropertyID aPropID,
                                                  Element* aTargetElement,
                                                  nsPresContext* aPresContext,
                                                  ComputedStyle* aComputedStyle,
                                                  const nsAString& aString) {
  ServoAnimationValues result;

  Document* doc = aTargetElement->GetComposedDoc();
  if (!doc) {
    return result;
  }

  // Parse property
  ServoCSSParser::ParsingEnvironment env =
      ServoCSSParser::GetParsingEnvironment(doc);
  RefPtr<RawServoDeclarationBlock> servoDeclarationBlock =
      ServoCSSParser::ParseProperty(aPropID, aString, env,
                                    ParsingMode::AllowUnitlessLength |
                                        ParsingMode::AllowAllNumericValues);
  if (!servoDeclarationBlock) {
    return result;
  }

  // Compute value
  aPresContext->StyleSet()->GetAnimationValues(
      servoDeclarationBlock, aTargetElement, aComputedStyle, result);

  return result;
}

// static
void SMILCSSValueType::ValueFromString(nsCSSPropertyID aPropID,
                                       Element* aTargetElement,
                                       const nsAString& aString,
                                       SMILValue& aValue,
                                       bool* aIsContextSensitive) {
  MOZ_ASSERT(aValue.IsNull(), "Outparam should be null-typed");
  nsPresContext* presContext =
      nsContentUtils::GetContextForContent(aTargetElement);
  if (!presContext) {
    NS_WARNING("Not parsing animation value; unable to get PresContext");
    return;
  }

  Document* doc = aTargetElement->GetComposedDoc();
  if (doc && !nsStyleUtil::CSPAllowsInlineStyle(nullptr, doc->NodePrincipal(),
                                                nullptr, doc->GetDocumentURI(),
                                                0, 0, aString, nullptr)) {
    return;
  }

  RefPtr<ComputedStyle> computedStyle =
      nsComputedDOMStyle::GetComputedStyle(aTargetElement, nullptr);
  if (!computedStyle) {
    return;
  }

  ServoAnimationValues parsedValues = ValueFromStringHelper(
      aPropID, aTargetElement, presContext, computedStyle, aString);
  if (aIsContextSensitive) {
    // FIXME: Bug 1358955 - detect context-sensitive values and set this value
    // appropriately.
    *aIsContextSensitive = false;
  }

  if (!parsedValues.IsEmpty()) {
    sSingleton.Init(aValue);
    aValue.mU.mPtr = new ValueWrapper(aPropID, std::move(parsedValues));
  }
}

// static
SMILValue SMILCSSValueType::ValueFromAnimationValue(
    nsCSSPropertyID aPropID, Element* aTargetElement,
    const AnimationValue& aValue) {
  SMILValue result;

  Document* doc = aTargetElement->GetComposedDoc();
  // We'd like to avoid serializing |aValue| if possible, and since the
  // string passed to CSPAllowsInlineStyle is only used for reporting violations
  // and an intermediate CSS value is not likely to be particularly useful
  // in that case, we just use a generic placeholder string instead.
  static const nsLiteralString kPlaceholderText =
      NS_LITERAL_STRING("[SVG animation of CSS]");
  if (doc && !nsStyleUtil::CSPAllowsInlineStyle(
                 nullptr, doc->NodePrincipal(), nullptr, doc->GetDocumentURI(),
                 0, 0, kPlaceholderText, nullptr)) {
    return result;
  }

  sSingleton.Init(result);
  result.mU.mPtr = new ValueWrapper(aPropID, aValue);

  return result;
}

// static
bool SMILCSSValueType::SetPropertyValues(const SMILValue& aValue,
                                         DeclarationBlock& aDecl) {
  MOZ_ASSERT(aValue.mType == &SMILCSSValueType::sSingleton,
             "Unexpected SMIL value type");
  const ValueWrapper* wrapper = ExtractValueWrapper(aValue);
  if (!wrapper) {
    return false;
  }

  bool changed = false;
  for (const auto& value : wrapper->mServoValues) {
    changed |=
        Servo_DeclarationBlock_SetPropertyToAnimationValue(aDecl.Raw(), value);
  }

  return changed;
}

// static
nsCSSPropertyID SMILCSSValueType::PropertyFromValue(const SMILValue& aValue) {
  if (aValue.mType != &SMILCSSValueType::sSingleton) {
    return eCSSProperty_UNKNOWN;
  }

  const ValueWrapper* wrapper = ExtractValueWrapper(aValue);
  if (!wrapper) {
    return eCSSProperty_UNKNOWN;
  }

  return wrapper->mPropID;
}

// static
void SMILCSSValueType::FinalizeValue(SMILValue& aValue,
                                     const SMILValue& aValueToMatch) {
  MOZ_ASSERT(aValue.mType == aValueToMatch.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aValue.mType == &SMILCSSValueType::sSingleton,
             "Unexpected SMIL value type");

  ValueWrapper* valueWrapper = ExtractValueWrapper(aValue);
  // If |aValue| already has a value, there's nothing to do here.
  if (valueWrapper) {
    return;
  }

  const ValueWrapper* valueToMatchWrapper = ExtractValueWrapper(aValueToMatch);
  if (!valueToMatchWrapper) {
    MOZ_ASSERT_UNREACHABLE("Value to match is empty");
    return;
  }

  ServoAnimationValues zeroValues;
  zeroValues.SetCapacity(valueToMatchWrapper->mServoValues.Length());

  for (auto& valueToMatch : valueToMatchWrapper->mServoValues) {
    RefPtr<RawServoAnimationValue> zeroValue =
        Servo_AnimationValues_GetZeroValue(valueToMatch).Consume();
    if (!zeroValue) {
      return;
    }
    zeroValues.AppendElement(std::move(zeroValue));
  }
  aValue.mU.mPtr =
      new ValueWrapper(valueToMatchWrapper->mPropID, std::move(zeroValues));
}

}  // namespace mozilla
