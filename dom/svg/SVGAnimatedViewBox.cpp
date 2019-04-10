/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAnimatedViewBox.h"

#include "mozilla/Move.h"
#include "mozilla/SMILValue.h"
#include "mozilla/SVGContentUtils.h"
#include "nsCharSeparatedTokenizer.h"
#include "SVGViewBoxSMILType.h"
#include "nsTextFormatter.h"

using namespace mozilla::dom;

namespace mozilla {

#define NUM_VIEWBOX_COMPONENTS 4

/* Implementation of SVGViewBox methods */

bool SVGViewBox::operator==(const SVGViewBox& aOther) const {
  if (&aOther == this) return true;

  return (none && aOther.none) ||
         (!none && !aOther.none && x == aOther.x && y == aOther.y &&
          width == aOther.width && height == aOther.height);
}

/* static */
nsresult SVGViewBox::FromString(const nsAString& aStr, SVGViewBox* aViewBox) {
  if (aStr.EqualsLiteral("none")) {
    aViewBox->none = true;
    return NS_OK;
  }

  nsCharSeparatedTokenizerTemplate<nsContentUtils::IsHTMLWhitespace> tokenizer(
      aStr, ',', nsCharSeparatedTokenizer::SEPARATOR_OPTIONAL);
  float vals[NUM_VIEWBOX_COMPONENTS];
  uint32_t i;
  for (i = 0; i < NUM_VIEWBOX_COMPONENTS && tokenizer.hasMoreTokens(); ++i) {
    if (!SVGContentUtils::ParseNumber(tokenizer.nextToken(), vals[i])) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }
  }

  if (i != NUM_VIEWBOX_COMPONENTS ||             // Too few values.
      tokenizer.hasMoreTokens() ||               // Too many values.
      tokenizer.separatorAfterCurrentToken()) {  // Trailing comma.
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  aViewBox->x = vals[0];
  aViewBox->y = vals[1];
  aViewBox->width = vals[2];
  aViewBox->height = vals[3];
  aViewBox->none = false;

  return NS_OK;
}

/* Cycle collection macros for SVGAnimatedViewBox */

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(SVGAnimatedViewBox::DOMBaseVal,
                                               mSVGElement)
NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(SVGAnimatedViewBox::DOMAnimVal,
                                               mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(SVGAnimatedViewBox::DOMBaseVal)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SVGAnimatedViewBox::DOMBaseVal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(SVGAnimatedViewBox::DOMAnimVal)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SVGAnimatedViewBox::DOMAnimVal)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SVGAnimatedViewBox::DOMBaseVal)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SVGAnimatedViewBox::DOMAnimVal)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

static SVGAttrTearoffTable<SVGAnimatedViewBox, SVGAnimatedViewBox::DOMBaseVal>
    sBaseSVGViewBoxTearoffTable;
static SVGAttrTearoffTable<SVGAnimatedViewBox, SVGAnimatedViewBox::DOMAnimVal>
    sAnimSVGViewBoxTearoffTable;
SVGAttrTearoffTable<SVGAnimatedViewBox, SVGAnimatedRect>
    SVGAnimatedViewBox::sSVGAnimatedRectTearoffTable;

/* Implementation of SVGAnimatedViewBox methods */

void SVGAnimatedViewBox::Init() {
  mHasBaseVal = false;
  // We shouldn't use mBaseVal for rendering (its usages should be guarded with
  // "mHasBaseVal" checks), but just in case we do by accident, this will
  // ensure that we treat it as "none" and ignore its numeric values:
  mBaseVal.none = true;

  mAnimVal = nullptr;
}

bool SVGAnimatedViewBox::HasRect() const {
  // Check mAnimVal if we have one; otherwise, check mBaseVal if we have one;
  // otherwise, just return false (we clearly do not have a rect).
  const SVGViewBox* rect = mAnimVal;
  if (!rect) {
    if (!mHasBaseVal) {
      // no anim val, no base val --> no viewbox rect
      return false;
    }
    rect = &mBaseVal;
  }

  return !rect->none && rect->width >= 0 && rect->height >= 0;
}

void SVGAnimatedViewBox::SetAnimValue(const SVGViewBox& aRect,
                                      SVGElement* aSVGElement) {
  if (!mAnimVal) {
    // it's okay if allocation fails - and no point in reporting that
    mAnimVal = new SVGViewBox(aRect);
  } else {
    if (aRect == *mAnimVal) {
      return;
    }
    *mAnimVal = aRect;
  }
  aSVGElement->DidAnimateViewBox();
}

void SVGAnimatedViewBox::SetBaseValue(const SVGViewBox& aRect,
                                      SVGElement* aSVGElement) {
  if (!mHasBaseVal || mBaseVal == aRect) {
    // This method is used to set a single x, y, width
    // or height value. It can't create a base value
    // as the other components may be undefined. We record
    // the new value though, so as not to lose data.
    mBaseVal = aRect;
    return;
  }

  nsAttrValue emptyOrOldValue = aSVGElement->WillChangeViewBox();

  mBaseVal = aRect;
  mHasBaseVal = true;

  aSVGElement->DidChangeViewBox(emptyOrOldValue);
  if (mAnimVal) {
    aSVGElement->AnimationNeedsResample();
  }
}

nsresult SVGAnimatedViewBox::SetBaseValueString(const nsAString& aValue,
                                                SVGElement* aSVGElement,
                                                bool aDoSetAttr) {
  SVGViewBox viewBox;

  nsresult rv = SVGViewBox::FromString(aValue, &viewBox);
  if (NS_FAILED(rv)) {
    return rv;
  }
  // Comparison against mBaseVal is only valid if we currently have a base val.
  if (mHasBaseVal && viewBox == mBaseVal) {
    return NS_OK;
  }

  nsAttrValue emptyOrOldValue;
  if (aDoSetAttr) {
    emptyOrOldValue = aSVGElement->WillChangeViewBox();
  }
  mHasBaseVal = true;
  mBaseVal = viewBox;

  if (aDoSetAttr) {
    aSVGElement->DidChangeViewBox(emptyOrOldValue);
  }
  if (mAnimVal) {
    aSVGElement->AnimationNeedsResample();
  }
  return NS_OK;
}

void SVGAnimatedViewBox::GetBaseValueString(nsAString& aValue) const {
  if (mBaseVal.none) {
    aValue.AssignLiteral("none");
    return;
  }
  nsTextFormatter::ssprintf(aValue, u"%g %g %g %g", (double)mBaseVal.x,
                            (double)mBaseVal.y, (double)mBaseVal.width,
                            (double)mBaseVal.height);
}

already_AddRefed<SVGAnimatedRect> SVGAnimatedViewBox::ToSVGAnimatedRect(
    SVGElement* aSVGElement) {
  RefPtr<SVGAnimatedRect> domAnimatedRect =
      sSVGAnimatedRectTearoffTable.GetTearoff(this);
  if (!domAnimatedRect) {
    domAnimatedRect = new SVGAnimatedRect(this, aSVGElement);
    sSVGAnimatedRectTearoffTable.AddTearoff(this, domAnimatedRect);
  }

  return domAnimatedRect.forget();
}

already_AddRefed<SVGIRect> SVGAnimatedViewBox::ToDOMBaseVal(
    SVGElement* aSVGElement) {
  if (!mHasBaseVal || mBaseVal.none) {
    return nullptr;
  }

  RefPtr<DOMBaseVal> domBaseVal = sBaseSVGViewBoxTearoffTable.GetTearoff(this);
  if (!domBaseVal) {
    domBaseVal = new DOMBaseVal(this, aSVGElement);
    sBaseSVGViewBoxTearoffTable.AddTearoff(this, domBaseVal);
  }

  return domBaseVal.forget();
}

SVGAnimatedViewBox::DOMBaseVal::~DOMBaseVal() {
  sBaseSVGViewBoxTearoffTable.RemoveTearoff(mVal);
}

already_AddRefed<SVGIRect> SVGAnimatedViewBox::ToDOMAnimVal(
    SVGElement* aSVGElement) {
  if ((mAnimVal && mAnimVal->none) ||
      (!mAnimVal && (!mHasBaseVal || mBaseVal.none))) {
    return nullptr;
  }

  RefPtr<DOMAnimVal> domAnimVal = sAnimSVGViewBoxTearoffTable.GetTearoff(this);
  if (!domAnimVal) {
    domAnimVal = new DOMAnimVal(this, aSVGElement);
    sAnimSVGViewBoxTearoffTable.AddTearoff(this, domAnimVal);
  }

  return domAnimVal.forget();
}

SVGAnimatedViewBox::DOMAnimVal::~DOMAnimVal() {
  sAnimSVGViewBoxTearoffTable.RemoveTearoff(mVal);
}

void SVGAnimatedViewBox::DOMBaseVal::SetX(float aX, ErrorResult& aRv) {
  SVGViewBox rect = mVal->GetBaseValue();
  rect.x = aX;
  mVal->SetBaseValue(rect, mSVGElement);
}

void SVGAnimatedViewBox::DOMBaseVal::SetY(float aY, ErrorResult& aRv) {
  SVGViewBox rect = mVal->GetBaseValue();
  rect.y = aY;
  mVal->SetBaseValue(rect, mSVGElement);
}

void SVGAnimatedViewBox::DOMBaseVal::SetWidth(float aWidth, ErrorResult& aRv) {
  SVGViewBox rect = mVal->GetBaseValue();
  rect.width = aWidth;
  mVal->SetBaseValue(rect, mSVGElement);
}

void SVGAnimatedViewBox::DOMBaseVal::SetHeight(float aHeight,
                                               ErrorResult& aRv) {
  SVGViewBox rect = mVal->GetBaseValue();
  rect.height = aHeight;
  mVal->SetBaseValue(rect, mSVGElement);
}

UniquePtr<SMILAttr> SVGAnimatedViewBox::ToSMILAttr(SVGElement* aSVGElement) {
  return MakeUnique<SMILViewBox>(this, aSVGElement);
}

nsresult SVGAnimatedViewBox::SMILViewBox ::ValueFromString(
    const nsAString& aStr, const SVGAnimationElement* /*aSrcElement*/,
    SMILValue& aValue, bool& aPreventCachingOfSandwich) const {
  SVGViewBox viewBox;
  nsresult res = SVGViewBox::FromString(aStr, &viewBox);
  if (NS_FAILED(res)) {
    return res;
  }
  SMILValue val(&SVGViewBoxSMILType::sSingleton);
  *static_cast<SVGViewBox*>(val.mU.mPtr) = viewBox;
  aValue = std::move(val);
  aPreventCachingOfSandwich = false;

  return NS_OK;
}

SMILValue SVGAnimatedViewBox::SMILViewBox::GetBaseValue() const {
  SMILValue val(&SVGViewBoxSMILType::sSingleton);
  *static_cast<SVGViewBox*>(val.mU.mPtr) = mVal->mBaseVal;
  return val;
}

void SVGAnimatedViewBox::SMILViewBox::ClearAnimValue() {
  if (mVal->mAnimVal) {
    mVal->mAnimVal = nullptr;
    mSVGElement->DidAnimateViewBox();
  }
}

nsresult SVGAnimatedViewBox::SMILViewBox::SetAnimValue(
    const SMILValue& aValue) {
  NS_ASSERTION(aValue.mType == &SVGViewBoxSMILType::sSingleton,
               "Unexpected type to assign animated value");
  if (aValue.mType == &SVGViewBoxSMILType::sSingleton) {
    SVGViewBox& vb = *static_cast<SVGViewBox*>(aValue.mU.mPtr);
    mVal->SetAnimValue(vb, mSVGElement);
  }
  return NS_OK;
}

}  // namespace mozilla
