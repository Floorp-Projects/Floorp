/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAnimatedTransformList.h"

#include <utility>

#include "DOMSVGAnimatedTransformList.h"
#include "SVGTransform.h"
#include "SVGTransformListSMILType.h"
#include "mozilla/SMILValue.h"
#include "mozilla/SVGContentUtils.h"
#include "mozilla/dom/MutationEventBinding.h"
#include "mozilla/dom/SVGAnimationElement.h"
#include "nsCharSeparatedTokenizer.h"

using namespace mozilla::dom;
using namespace mozilla::dom::SVGTransform_Binding;

namespace mozilla {

nsresult SVGAnimatedTransformList::SetBaseValueString(const nsAString& aValue,
                                                      SVGElement* aSVGElement) {
  SVGTransformList newBaseValue;
  nsresult rv = newBaseValue.SetValueFromString(aValue);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return SetBaseValue(newBaseValue, aSVGElement);
}

nsresult SVGAnimatedTransformList::SetBaseValue(const SVGTransformList& aValue,
                                                SVGElement* aSVGElement) {
  DOMSVGAnimatedTransformList* domWrapper =
      DOMSVGAnimatedTransformList::GetDOMWrapperIfExists(this);
  if (domWrapper) {
    // We must send this notification *before* changing mBaseVal! If the length
    // of our baseVal is being reduced, our baseVal's DOM wrapper list may have
    // to remove DOM items from itself, and any removed DOM items need to copy
    // their internal counterpart values *before* we change them.
    //
    domWrapper->InternalBaseValListWillChangeLengthTo(aValue.Length());
  }

  // (This bool will be copied to our member-var, if attr-change succeeds.)
  bool hadTransform = HasTransform();

  // We don't need to call DidChange* here - we're only called by
  // SVGElement::ParseAttribute under Element::SetAttr,
  // which takes care of notifying.

  nsresult rv = mBaseVal.CopyFrom(aValue);
  if (NS_FAILED(rv) && domWrapper) {
    // Attempting to increase mBaseVal's length failed - reduce domWrapper
    // back to the same length:
    domWrapper->InternalBaseValListWillChangeLengthTo(mBaseVal.Length());
  } else {
    mIsAttrSet = true;
    // We only need to treat this as a creation or removal of a transform if the
    // frame already exists and it didn't have an existing one.
    mCreatedOrRemovedOnLastChange =
        aSVGElement->GetPrimaryFrame() && !hadTransform;
  }
  return rv;
}

void SVGAnimatedTransformList::ClearBaseValue() {
  mCreatedOrRemovedOnLastChange = !HasTransform();

  DOMSVGAnimatedTransformList* domWrapper =
      DOMSVGAnimatedTransformList::GetDOMWrapperIfExists(this);
  if (domWrapper) {
    // We must send this notification *before* changing mBaseVal! (See above.)
    domWrapper->InternalBaseValListWillChangeLengthTo(0);
  }
  mBaseVal.Clear();
  mIsAttrSet = false;
  // Caller notifies
}

nsresult SVGAnimatedTransformList::SetAnimValue(const SVGTransformList& aValue,
                                                SVGElement* aElement) {
  bool prevSet = HasTransform() || aElement->GetAnimateMotionTransform();
  DOMSVGAnimatedTransformList* domWrapper =
      DOMSVGAnimatedTransformList::GetDOMWrapperIfExists(this);
  if (domWrapper) {
    // A new animation may totally change the number of items in the animVal
    // list, replacing what was essentially a mirror of the baseVal list, or
    // else replacing and overriding an existing animation. When this happens
    // we must try and keep our animVal's DOM wrapper in sync (see the comment
    // in DOMSVGAnimatedTransformList::InternalBaseValListWillChangeLengthTo).
    //
    // It's not possible for us to reliably distinguish between calls to this
    // method that are setting a new sample for an existing animation, and
    // calls that are setting the first sample of an animation that will
    // override an existing animation. Happily it's cheap to just blindly
    // notify our animVal's DOM wrapper of its internal counterpart's new value
    // each time this method is called, so that's what we do.
    //
    // Note that we must send this notification *before* setting or changing
    // mAnimVal! (See the comment in SetBaseValueString above.)
    //
    domWrapper->InternalAnimValListWillChangeLengthTo(aValue.Length());
  }
  if (!mAnimVal) {
    mAnimVal = MakeUnique<SVGTransformList>();
  }
  nsresult rv = mAnimVal->CopyFrom(aValue);
  if (NS_FAILED(rv)) {
    // OOM. We clear the animation, and, importantly, ClearAnimValue() ensures
    // that mAnimVal and its DOM wrapper (if any) will have the same length!
    ClearAnimValue(aElement);
    return rv;
  }
  int32_t modType;
  if (prevSet) {
    modType = MutationEvent_Binding::MODIFICATION;
  } else {
    modType = MutationEvent_Binding::ADDITION;
  }
  mCreatedOrRemovedOnLastChange = modType == MutationEvent_Binding::ADDITION;
  aElement->DidAnimateTransformList(modType);
  return NS_OK;
}

void SVGAnimatedTransformList::ClearAnimValue(SVGElement* aElement) {
  DOMSVGAnimatedTransformList* domWrapper =
      DOMSVGAnimatedTransformList::GetDOMWrapperIfExists(this);
  if (domWrapper) {
    // When all animation ends, animVal simply mirrors baseVal, which may have
    // a different number of items to the last active animated value. We must
    // keep the length of our animVal's DOM wrapper list in sync, and again we
    // must do that before touching mAnimVal. See comments above.
    //
    domWrapper->InternalAnimValListWillChangeLengthTo(mBaseVal.Length());
  }
  mAnimVal = nullptr;
  int32_t modType;
  if (HasTransform() || aElement->GetAnimateMotionTransform()) {
    modType = MutationEvent_Binding::MODIFICATION;
  } else {
    modType = MutationEvent_Binding::REMOVAL;
  }
  mCreatedOrRemovedOnLastChange = modType == MutationEvent_Binding::REMOVAL;
  aElement->DidAnimateTransformList(modType);
}

bool SVGAnimatedTransformList::IsExplicitlySet() const {
  // Like other methods of this name, we need to know when a transform value has
  // been explicitly set.
  //
  // There are three ways an animated list can become set:
  // 1) Markup -- we set mIsAttrSet to true on any successful call to
  //    SetBaseValueString and clear it on ClearBaseValue (as called by
  //    SVGElement::UnsetAttr or a failed SVGElement::ParseAttribute)
  // 2) DOM call -- simply fetching the baseVal doesn't mean the transform value
  //    has been set. It is set if that baseVal has one or more transforms in
  //    the list.
  // 3) Animation -- which will cause the mAnimVal member to be allocated
  return mIsAttrSet || !mBaseVal.IsEmpty() || mAnimVal;
}

UniquePtr<SMILAttr> SVGAnimatedTransformList::ToSMILAttr(
    SVGElement* aSVGElement) {
  return MakeUnique<SMILAnimatedTransformList>(this, aSVGElement);
}

nsresult SVGAnimatedTransformList::SMILAnimatedTransformList::ValueFromString(
    const nsAString& aStr, const dom::SVGAnimationElement* aSrcElement,
    SMILValue& aValue, bool& aPreventCachingOfSandwich) const {
  NS_ENSURE_TRUE(aSrcElement, NS_ERROR_FAILURE);
  MOZ_ASSERT(aValue.IsNull(),
             "aValue should have been cleared before calling ValueFromString");

  const nsAttrValue* typeAttr = aSrcElement->GetParsedAttr(nsGkAtoms::type);
  const nsAtom* transformType = nsGkAtoms::translate;  // default val
  if (typeAttr) {
    if (typeAttr->Type() != nsAttrValue::eAtom) {
      // Recognized values of |type| are parsed as an atom -- so if we have
      // something other than an atom, then we know already our |type| is
      // invalid.
      return NS_ERROR_FAILURE;
    }
    transformType = typeAttr->GetAtomValue();
  }

  ParseValue(aStr, transformType, aValue);
  aPreventCachingOfSandwich = false;
  return aValue.IsNull() ? NS_ERROR_FAILURE : NS_OK;
}

void SVGAnimatedTransformList::SMILAnimatedTransformList::ParseValue(
    const nsAString& aSpec, const nsAtom* aTransformType, SMILValue& aResult) {
  MOZ_ASSERT(aResult.IsNull(), "Unexpected type for SMIL value");

  static_assert(SVGTransformSMILData::NUM_SIMPLE_PARAMS == 3,
                "nsSVGSMILTransform constructor should be expecting array "
                "with 3 params");

  float params[3] = {0.f};
  int32_t numParsed = ParseParameterList(aSpec, params, 3);
  uint16_t transformType;

  if (aTransformType == nsGkAtoms::translate) {
    // tx [ty=0]
    if (numParsed != 1 && numParsed != 2) return;
    transformType = SVG_TRANSFORM_TRANSLATE;
  } else if (aTransformType == nsGkAtoms::scale) {
    // sx [sy=sx]
    if (numParsed != 1 && numParsed != 2) return;
    if (numParsed == 1) {
      params[1] = params[0];
    }
    transformType = SVG_TRANSFORM_SCALE;
  } else if (aTransformType == nsGkAtoms::rotate) {
    // r [cx=0 cy=0]
    if (numParsed != 1 && numParsed != 3) return;
    transformType = SVG_TRANSFORM_ROTATE;
  } else if (aTransformType == nsGkAtoms::skewX) {
    // x-angle
    if (numParsed != 1) return;
    transformType = SVG_TRANSFORM_SKEWX;
  } else if (aTransformType == nsGkAtoms::skewY) {
    // y-angle
    if (numParsed != 1) return;
    transformType = SVG_TRANSFORM_SKEWY;
  } else {
    return;
  }

  SMILValue val(SVGTransformListSMILType::Singleton());
  SVGTransformSMILData transform(transformType, params);
  if (NS_FAILED(SVGTransformListSMILType::AppendTransform(transform, val))) {
    return;  // OOM
  }

  // Success! Populate our outparam with parsed value.
  aResult = std::move(val);
}

int32_t SVGAnimatedTransformList::SMILAnimatedTransformList::ParseParameterList(
    const nsAString& aSpec, float* aVars, int32_t aNVars) {
  nsCharSeparatedTokenizerTemplate<nsContentUtils::IsHTMLWhitespace> tokenizer(
      aSpec, ',', nsCharSeparatedTokenizer::SEPARATOR_OPTIONAL);

  int numArgsFound = 0;

  while (tokenizer.hasMoreTokens()) {
    float f;
    if (!SVGContentUtils::ParseNumber(tokenizer.nextToken(), f)) {
      return -1;
    }
    if (numArgsFound < aNVars) {
      aVars[numArgsFound] = f;
    }
    numArgsFound++;
  }
  return numArgsFound;
}

SMILValue SVGAnimatedTransformList::SMILAnimatedTransformList::GetBaseValue()
    const {
  // To benefit from Return Value Optimization and avoid copy constructor calls
  // due to our use of return-by-value, we must return the exact same object
  // from ALL return points. This function must only return THIS variable:
  SMILValue val(SVGTransformListSMILType::Singleton());
  if (!SVGTransformListSMILType::AppendTransforms(mVal->mBaseVal, val)) {
    val = SMILValue();
  }

  return val;
}

nsresult SVGAnimatedTransformList::SMILAnimatedTransformList::SetAnimValue(
    const SMILValue& aNewAnimValue) {
  MOZ_ASSERT(aNewAnimValue.mType == SVGTransformListSMILType::Singleton(),
             "Unexpected type to assign animated value");
  SVGTransformList animVal;
  if (!SVGTransformListSMILType::GetTransforms(aNewAnimValue, animVal.mItems)) {
    return NS_ERROR_FAILURE;
  }

  return mVal->SetAnimValue(animVal, mElement);
}

void SVGAnimatedTransformList::SMILAnimatedTransformList::ClearAnimValue() {
  if (mVal->mAnimVal) {
    mVal->ClearAnimValue(mElement);
  }
}

}  // namespace mozilla
