/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAnimatedViewBox.h"

#include "mozAutoDocUpdate.h"
#include "mozilla/Maybe.h"
#include <utility>

#include "SVGViewBoxSMILType.h"
#include "mozilla/SMILValue.h"
#include "mozilla/SVGContentUtils.h"
#include "mozilla/dom/SVGRect.h"
#include "nsCharSeparatedTokenizer.h"
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

  nsCharSeparatedTokenizerTemplate<nsContentUtils::IsHTMLWhitespace,
                                   nsTokenizerFlags::SeparatorOptional>
      tokenizer(aStr, ',');
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

static SVGAttrTearoffTable<SVGAnimatedViewBox, SVGRect>
    sBaseSVGViewBoxTearoffTable;
static SVGAttrTearoffTable<SVGAnimatedViewBox, SVGRect>
    sAnimSVGViewBoxTearoffTable;
SVGAttrTearoffTable<SVGAnimatedViewBox, SVGAnimatedRect>
    SVGAnimatedViewBox::sSVGAnimatedRectTearoffTable;

//----------------------------------------------------------------------
// Helper class: AutoChangeViewBoxNotifier
// Stack-based helper class to pair calls to WillChangeViewBox and
// DidChangeViewBox.
class MOZ_RAII AutoChangeViewBoxNotifier {
 public:
  AutoChangeViewBoxNotifier(SVGAnimatedViewBox* aViewBox,
                            SVGElement* aSVGElement, bool aDoSetAttr = true)
      : mViewBox(aViewBox), mSVGElement(aSVGElement), mDoSetAttr(aDoSetAttr) {
    MOZ_ASSERT(mViewBox, "Expecting non-null viewBox");
    MOZ_ASSERT(mSVGElement, "Expecting non-null element");

    if (mDoSetAttr) {
      mUpdateBatch.emplace(aSVGElement->GetComposedDoc(), true);
      mEmptyOrOldValue = mSVGElement->WillChangeViewBox(mUpdateBatch.ref());
    }
  }

  ~AutoChangeViewBoxNotifier() {
    if (mDoSetAttr) {
      mSVGElement->DidChangeViewBox(mEmptyOrOldValue, mUpdateBatch.ref());
    }
    if (mViewBox->mAnimVal) {
      mSVGElement->AnimationNeedsResample();
    }
  }

 private:
  SVGAnimatedViewBox* const mViewBox;
  SVGElement* const mSVGElement;
  Maybe<mozAutoDocUpdate> mUpdateBatch;
  nsAttrValue mEmptyOrOldValue;
  bool mDoSetAttr;
};

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
  const SVGViewBox* rect = mAnimVal.get();
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
    mAnimVal = MakeUnique<SVGViewBox>(aRect);
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

  AutoChangeViewBoxNotifier notifier(this, aSVGElement);

  mBaseVal = aRect;
  mHasBaseVal = true;
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

  AutoChangeViewBoxNotifier notifier(this, aSVGElement, aDoSetAttr);
  mHasBaseVal = true;
  mBaseVal = viewBox;

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

already_AddRefed<SVGRect> SVGAnimatedViewBox::ToDOMBaseVal(
    SVGElement* aSVGElement) {
  if (!mHasBaseVal || mBaseVal.none) {
    return nullptr;
  }

  RefPtr<SVGRect> domBaseVal = sBaseSVGViewBoxTearoffTable.GetTearoff(this);
  if (!domBaseVal) {
    domBaseVal = new SVGRect(this, aSVGElement, SVGRect::RectType::BaseValue);
    sBaseSVGViewBoxTearoffTable.AddTearoff(this, domBaseVal);
  }

  return domBaseVal.forget();
}

SVGRect::~SVGRect() {
  switch (mType) {
    case RectType::BaseValue:
      sBaseSVGViewBoxTearoffTable.RemoveTearoff(mVal);
      break;
    case RectType::AnimValue:
      sAnimSVGViewBoxTearoffTable.RemoveTearoff(mVal);
      break;
    default:
      break;
  }
}

already_AddRefed<SVGRect> SVGAnimatedViewBox::ToDOMAnimVal(
    SVGElement* aSVGElement) {
  if ((mAnimVal && mAnimVal->none) ||
      (!mAnimVal && (!mHasBaseVal || mBaseVal.none))) {
    return nullptr;
  }

  RefPtr<SVGRect> domAnimVal = sAnimSVGViewBoxTearoffTable.GetTearoff(this);
  if (!domAnimVal) {
    domAnimVal = new SVGRect(this, aSVGElement, SVGRect::RectType::AnimValue);
    sAnimSVGViewBoxTearoffTable.AddTearoff(this, domAnimVal);
  }

  return domAnimVal.forget();
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
