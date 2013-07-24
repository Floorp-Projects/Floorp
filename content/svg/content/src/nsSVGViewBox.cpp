/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGViewBox.h"
#include "prdtoa.h"
#include "nsTextFormatter.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsMathUtils.h"
#include "nsSMILValue.h"
#include "SVGContentUtils.h"
#include "SVGViewBoxSMILType.h"
#include "nsAttrValueInlines.h"

#define NUM_VIEWBOX_COMPONENTS 4
using namespace mozilla;

/* Implementation of nsSVGViewBoxRect methods */

bool
nsSVGViewBoxRect::operator==(const nsSVGViewBoxRect& aOther) const
{
  if (&aOther == this)
    return true;

  return (none && aOther.none) ||
    (!none && !aOther.none &&
     x == aOther.x &&
     y == aOther.y &&
     width == aOther.width &&
     height == aOther.height);
}

/* Cycle collection macros for nsSVGViewBox */

NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(nsSVGViewBox::DOMBaseVal, mSVGElement)
NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(nsSVGViewBox::DOMAnimVal, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGViewBox::DOMBaseVal)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGViewBox::DOMBaseVal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsSVGViewBox::DOMAnimVal)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsSVGViewBox::DOMAnimVal)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGViewBox::DOMBaseVal)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSVGViewBox::DOMAnimVal)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

static nsSVGAttrTearoffTable<nsSVGViewBox, nsSVGViewBox::DOMBaseVal>
  sBaseSVGViewBoxTearoffTable;
static nsSVGAttrTearoffTable<nsSVGViewBox, nsSVGViewBox::DOMAnimVal>
  sAnimSVGViewBoxTearoffTable;
nsSVGAttrTearoffTable<nsSVGViewBox, dom::SVGAnimatedRect>
  nsSVGViewBox::sSVGAnimatedRectTearoffTable;


/* Implementation of nsSVGViewBox methods */

void
nsSVGViewBox::Init()
{
  mHasBaseVal = false;
  mAnimVal = nullptr;
}

void
nsSVGViewBox::SetAnimValue(const nsSVGViewBoxRect& aRect,
                           nsSVGElement *aSVGElement)
{
  if (!mAnimVal) {
    // it's okay if allocation fails - and no point in reporting that
    mAnimVal = new nsSVGViewBoxRect(aRect);
  } else {
    if (aRect == *mAnimVal) {
      return;
    }
    *mAnimVal = aRect;
  }
  aSVGElement->DidAnimateViewBox();
}

void
nsSVGViewBox::SetBaseValue(const nsSVGViewBoxRect& aRect,
                           nsSVGElement *aSVGElement)
{
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

static nsresult
ToSVGViewBoxRect(const nsAString& aStr, nsSVGViewBoxRect *aViewBox)
{
  if (aStr.EqualsLiteral("none")) {
    aViewBox->none = true;
    return NS_OK;
  }

  nsCharSeparatedTokenizerTemplate<IsSVGWhitespace>
    tokenizer(aStr, ',',
              nsCharSeparatedTokenizer::SEPARATOR_OPTIONAL);
  float vals[NUM_VIEWBOX_COMPONENTS];
  uint32_t i;
  for (i = 0; i < NUM_VIEWBOX_COMPONENTS && tokenizer.hasMoreTokens(); ++i) {
    NS_ConvertUTF16toUTF8 utf8Token(tokenizer.nextToken());
    const char *token = utf8Token.get();
    if (*token == '\0') {
      return NS_ERROR_DOM_SYNTAX_ERR; // empty string (e.g. two commas in a row)
    }

    char *end;
    vals[i] = float(PR_strtod(token, &end));
    if (*end != '\0' || !NS_finite(vals[i])) {
      return NS_ERROR_DOM_SYNTAX_ERR; // parse error
    }
  }

  if (i != NUM_VIEWBOX_COMPONENTS ||              // Too few values.
      tokenizer.hasMoreTokens() ||                // Too many values.
      tokenizer.lastTokenEndedWithSeparator()) {  // Trailing comma.
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  aViewBox->x = vals[0];
  aViewBox->y = vals[1];
  aViewBox->width = vals[2];
  aViewBox->height = vals[3];
  aViewBox->none = false;

  return NS_OK;
}

nsresult
nsSVGViewBox::SetBaseValueString(const nsAString& aValue,
                                 nsSVGElement *aSVGElement,
                                 bool aDoSetAttr)
{
  nsSVGViewBoxRect viewBox;

  nsresult rv = ToSVGViewBoxRect(aValue, &viewBox);
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

void
nsSVGViewBox::GetBaseValueString(nsAString& aValue) const
{
  if (mBaseVal.none) {
    aValue.AssignLiteral("none");
    return;
  }
  PRUnichar buf[200];
  nsTextFormatter::snprintf(buf, sizeof(buf)/sizeof(PRUnichar),
                            NS_LITERAL_STRING("%g %g %g %g").get(),
                            (double)mBaseVal.x, (double)mBaseVal.y,
                            (double)mBaseVal.width, (double)mBaseVal.height);
  aValue.Assign(buf);
}


already_AddRefed<dom::SVGAnimatedRect>
nsSVGViewBox::ToSVGAnimatedRect(nsSVGElement* aSVGElement)
{
  nsRefPtr<dom::SVGAnimatedRect> domAnimatedRect =
    sSVGAnimatedRectTearoffTable.GetTearoff(this);
  if (!domAnimatedRect) {
    domAnimatedRect = new dom::SVGAnimatedRect(this, aSVGElement);
    sSVGAnimatedRectTearoffTable.AddTearoff(this, domAnimatedRect);
  }

  return domAnimatedRect.forget();
}

already_AddRefed<dom::SVGIRect>
nsSVGViewBox::ToDOMBaseVal(nsSVGElement *aSVGElement)
{
  if (!mHasBaseVal || mBaseVal.none) {
    return nullptr;
  }

  nsRefPtr<DOMBaseVal> domBaseVal =
    sBaseSVGViewBoxTearoffTable.GetTearoff(this);
  if (!domBaseVal) {
    domBaseVal = new DOMBaseVal(this, aSVGElement);
    sBaseSVGViewBoxTearoffTable.AddTearoff(this, domBaseVal);
  }

 return domBaseVal.forget();
}

nsSVGViewBox::DOMBaseVal::~DOMBaseVal()
{
  sBaseSVGViewBoxTearoffTable.RemoveTearoff(mVal);
}

already_AddRefed<dom::SVGIRect>
nsSVGViewBox::ToDOMAnimVal(nsSVGElement *aSVGElement)
{
  if ((mAnimVal && mAnimVal->none) ||
      (!mAnimVal && (!mHasBaseVal || mBaseVal.none))) {
    return nullptr;
  }

  nsRefPtr<DOMAnimVal> domAnimVal =
    sAnimSVGViewBoxTearoffTable.GetTearoff(this);
  if (!domAnimVal) {
    domAnimVal = new DOMAnimVal(this, aSVGElement);
    sAnimSVGViewBoxTearoffTable.AddTearoff(this, domAnimVal);
  }

  return domAnimVal.forget();
}

nsSVGViewBox::DOMAnimVal::~DOMAnimVal()
{
  sAnimSVGViewBoxTearoffTable.RemoveTearoff(mVal);
}

void
nsSVGViewBox::DOMBaseVal::SetX(float aX, ErrorResult& aRv)
{
  nsSVGViewBoxRect rect = mVal->GetBaseValue();
  rect.x = aX;
  mVal->SetBaseValue(rect, mSVGElement);
}

void
nsSVGViewBox::DOMBaseVal::SetY(float aY, ErrorResult& aRv)
{
  nsSVGViewBoxRect rect = mVal->GetBaseValue();
  rect.y = aY;
  mVal->SetBaseValue(rect, mSVGElement);
}

void
nsSVGViewBox::DOMBaseVal::SetWidth(float aWidth, ErrorResult& aRv)
{
  nsSVGViewBoxRect rect = mVal->GetBaseValue();
  rect.width = aWidth;
  mVal->SetBaseValue(rect, mSVGElement);
}

void
nsSVGViewBox::DOMBaseVal::SetHeight(float aHeight, ErrorResult& aRv)
{
  nsSVGViewBoxRect rect = mVal->GetBaseValue();
  rect.height = aHeight;
  mVal->SetBaseValue(rect, mSVGElement);
}

nsISMILAttr*
nsSVGViewBox::ToSMILAttr(nsSVGElement *aSVGElement)
{
  return new SMILViewBox(this, aSVGElement);
}

nsresult
nsSVGViewBox::SMILViewBox
            ::ValueFromString(const nsAString& aStr,
                              const dom::SVGAnimationElement* /*aSrcElement*/,
                              nsSMILValue& aValue,
                              bool& aPreventCachingOfSandwich) const
{
  nsSVGViewBoxRect viewBox;
  nsresult res = ToSVGViewBoxRect(aStr, &viewBox);
  if (NS_FAILED(res)) {
    return res;
  }
  nsSMILValue val(&SVGViewBoxSMILType::sSingleton);
  *static_cast<nsSVGViewBoxRect*>(val.mU.mPtr) = viewBox;
  aValue.Swap(val);
  aPreventCachingOfSandwich = false;
  
  return NS_OK;
}

nsSMILValue
nsSVGViewBox::SMILViewBox::GetBaseValue() const
{
  nsSMILValue val(&SVGViewBoxSMILType::sSingleton);
  *static_cast<nsSVGViewBoxRect*>(val.mU.mPtr) = mVal->mBaseVal;
  return val;
}

void
nsSVGViewBox::SMILViewBox::ClearAnimValue()
{
  if (mVal->mAnimVal) {
    mVal->mAnimVal = nullptr;
    mSVGElement->DidAnimateViewBox();
  }
}

nsresult
nsSVGViewBox::SMILViewBox::SetAnimValue(const nsSMILValue& aValue)
{
  NS_ASSERTION(aValue.mType == &SVGViewBoxSMILType::sSingleton,
               "Unexpected type to assign animated value");
  if (aValue.mType == &SVGViewBoxSMILType::sSingleton) {
    nsSVGViewBoxRect &vb = *static_cast<nsSVGViewBoxRect*>(aValue.mU.mPtr);
    mVal->SetAnimValue(vb, mSVGElement);
  }
  return NS_OK;
}
