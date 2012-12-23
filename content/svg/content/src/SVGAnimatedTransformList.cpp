/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAnimatedTransformList.h"
#include "DOMSVGAnimatedTransformList.h"

#include "nsISMILAnimationElement.h"
#include "nsSMILValue.h"
#include "prdtoa.h"
#include "SVGContentUtils.h"
#include "SVGTransform.h"
#include "SVGTransformListSMILType.h"

namespace mozilla {

nsresult
SVGAnimatedTransformList::SetBaseValueString(const nsAString& aValue)
{
  SVGTransformList newBaseValue;
  nsresult rv = newBaseValue.SetValueFromString(aValue);
  if (NS_FAILED(rv)) {
    return rv;
  }

  DOMSVGAnimatedTransformList *domWrapper =
    DOMSVGAnimatedTransformList::GetDOMWrapperIfExists(this);
  if (domWrapper) {
    // We must send this notification *before* changing mBaseVal! If the length
    // of our baseVal is being reduced, our baseVal's DOM wrapper list may have
    // to remove DOM items from itself, and any removed DOM items need to copy
    // their internal counterpart values *before* we change them.
    //
    domWrapper->InternalBaseValListWillChangeLengthTo(newBaseValue.Length());
  }

  // We don't need to call DidChange* here - we're only called by
  // nsSVGElement::ParseAttribute under Element::SetAttr,
  // which takes care of notifying.

  rv = mBaseVal.CopyFrom(newBaseValue);
  if (NS_FAILED(rv) && domWrapper) {
    // Attempting to increase mBaseVal's length failed - reduce domWrapper
    // back to the same length:
    domWrapper->InternalBaseValListWillChangeLengthTo(mBaseVal.Length());
  } else {
    mIsAttrSet = true;
  }
  return rv;
}

void
SVGAnimatedTransformList::ClearBaseValue()
{
  DOMSVGAnimatedTransformList *domWrapper =
    DOMSVGAnimatedTransformList::GetDOMWrapperIfExists(this);
  if (domWrapper) {
    // We must send this notification *before* changing mBaseVal! (See above.)
    domWrapper->InternalBaseValListWillChangeLengthTo(0);
  }
  mBaseVal.Clear();
  mIsAttrSet = false;
  // Caller notifies
}

nsresult
SVGAnimatedTransformList::SetAnimValue(const SVGTransformList& aValue,
                                       nsSVGElement *aElement)
{
  DOMSVGAnimatedTransformList *domWrapper =
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
    mAnimVal = new SVGTransformList();
  }
  nsresult rv = mAnimVal->CopyFrom(aValue);
  if (NS_FAILED(rv)) {
    // OOM. We clear the animation, and, importantly, ClearAnimValue() ensures
    // that mAnimVal and its DOM wrapper (if any) will have the same length!
    ClearAnimValue(aElement);
    return rv;
  }
  aElement->DidAnimateTransformList();
  return NS_OK;
}

void
SVGAnimatedTransformList::ClearAnimValue(nsSVGElement *aElement)
{
  DOMSVGAnimatedTransformList *domWrapper =
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
  aElement->DidAnimateTransformList();
}

bool
SVGAnimatedTransformList::IsExplicitlySet() const
{
  // Like other methods of this name, we need to know when a transform value has
  // been explicitly set.
  //
  // There are three ways an animated list can become set:
  // 1) Markup -- we set mIsAttrSet to true on any successful call to
  //    SetBaseValueString and clear it on ClearBaseValue (as called by
  //    nsSVGElement::UnsetAttr or a failed nsSVGElement::ParseAttribute)
  // 2) DOM call -- simply fetching the baseVal doesn't mean the transform value
  //    has been set. It is set if that baseVal has one or more transforms in
  //    the list.
  // 3) Animation -- which will cause the mAnimVal member to be allocated
  return mIsAttrSet || !mBaseVal.IsEmpty() || mAnimVal;
}

nsISMILAttr*
SVGAnimatedTransformList::ToSMILAttr(nsSVGElement* aSVGElement)
{
  return new SMILAnimatedTransformList(this, aSVGElement);
}

nsresult
SVGAnimatedTransformList::SMILAnimatedTransformList::ValueFromString(
  const nsAString& aStr,
  const nsISMILAnimationElement* aSrcElement,
  nsSMILValue& aValue,
  bool& aPreventCachingOfSandwich) const
{
  NS_ENSURE_TRUE(aSrcElement, NS_ERROR_FAILURE);
  NS_ABORT_IF_FALSE(aValue.IsNull(),
    "aValue should have been cleared before calling ValueFromString");

  const nsAttrValue* typeAttr = aSrcElement->GetAnimAttr(nsGkAtoms::type);
  const nsIAtom* transformType = nsGkAtoms::translate; // default val
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

void
SVGAnimatedTransformList::SMILAnimatedTransformList::ParseValue(
  const nsAString& aSpec,
  const nsIAtom* aTransformType,
  nsSMILValue& aResult)
{
  NS_ABORT_IF_FALSE(aResult.IsNull(), "Unexpected type for SMIL value");

  // nsSVGSMILTransform constructor should be expecting array with 3 params
  PR_STATIC_ASSERT(SVGTransformSMILData::NUM_SIMPLE_PARAMS == 3);

  float params[3] = { 0.f };
  int32_t numParsed = ParseParameterList(aSpec, params, 3);
  uint16_t transformType;

  if (aTransformType == nsGkAtoms::translate) {
    // tx [ty=0]
    if (numParsed != 1 && numParsed != 2)
      return;
    transformType = SVG_TRANSFORM_TRANSLATE;
  } else if (aTransformType == nsGkAtoms::scale) {
    // sx [sy=sx]
    if (numParsed != 1 && numParsed != 2)
      return;
    if (numParsed == 1) {
      params[1] = params[0];
    }
    transformType = SVG_TRANSFORM_SCALE;
  } else if (aTransformType == nsGkAtoms::rotate) {
    // r [cx=0 cy=0]
    if (numParsed != 1 && numParsed != 3)
      return;
    transformType = SVG_TRANSFORM_ROTATE;
  } else if (aTransformType == nsGkAtoms::skewX) {
    // x-angle
    if (numParsed != 1)
      return;
    transformType = SVG_TRANSFORM_SKEWX;
  } else if (aTransformType == nsGkAtoms::skewY) {
    // y-angle
    if (numParsed != 1)
      return;
    transformType = SVG_TRANSFORM_SKEWY;
  } else {
    return;
  }

  nsSMILValue val(&SVGTransformListSMILType::sSingleton);
  SVGTransformSMILData transform(transformType, params);
  if (NS_FAILED(SVGTransformListSMILType::AppendTransform(transform, val))) {
    return; // OOM
  }

  // Success! Populate our outparam with parsed value.
  aResult.Swap(val);
}

namespace
{
  inline void
  SkipWsp(nsACString::const_iterator& aIter,
          const nsACString::const_iterator& aIterEnd)
  {
    while (aIter != aIterEnd && IsSVGWhitespace(*aIter))
      ++aIter;
  }
} // end anonymous namespace block

int32_t
SVGAnimatedTransformList::SMILAnimatedTransformList::ParseParameterList(
  const nsAString& aSpec,
  float* aVars,
  int32_t aNVars)
{
  NS_ConvertUTF16toUTF8 spec(aSpec);

  nsACString::const_iterator start, end;
  spec.BeginReading(start);
  spec.EndReading(end);

  SkipWsp(start, end);

  int numArgsFound = 0;

  while (start != end) {
    char const *arg = start.get();
    char *argend;
    float f = float(PR_strtod(arg, &argend));
    if (arg == argend || argend > end.get() || !NS_finite(f))
      return -1;

    if (numArgsFound < aNVars) {
      aVars[numArgsFound] = f;
    }

    start.advance(argend - arg);
    numArgsFound++;

    SkipWsp(start, end);
    if (*start == ',') {
      ++start;
      SkipWsp(start, end);
    }
  }

  return numArgsFound;
}

nsSMILValue
SVGAnimatedTransformList::SMILAnimatedTransformList::GetBaseValue() const
{
  // To benefit from Return Value Optimization and avoid copy constructor calls
  // due to our use of return-by-value, we must return the exact same object
  // from ALL return points. This function must only return THIS variable:
  nsSMILValue val(&SVGTransformListSMILType::sSingleton);
  if (!SVGTransformListSMILType::AppendTransforms(mVal->mBaseVal, val)) {
    val = nsSMILValue();
  }

  return val;
}

nsresult
SVGAnimatedTransformList::SMILAnimatedTransformList::SetAnimValue(
  const nsSMILValue& aNewAnimValue)
{
  NS_ABORT_IF_FALSE(
    aNewAnimValue.mType == &SVGTransformListSMILType::sSingleton,
    "Unexpected type to assign animated value");
  SVGTransformList animVal;
  if (!SVGTransformListSMILType::GetTransforms(aNewAnimValue,
                                               animVal.mItems)) {
    return NS_ERROR_FAILURE;
  }

  return mVal->SetAnimValue(animVal, mElement);
}

void
SVGAnimatedTransformList::SMILAnimatedTransformList::ClearAnimValue()
{
  if (mVal->mAnimVal) {
    mVal->ClearAnimValue(mElement);
  }
}

} // namespace mozilla
