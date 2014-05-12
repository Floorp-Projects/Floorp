/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "SVGAnimatedPreserveAspectRatio.h"
#include "mozilla/dom/SVGAnimatedPreserveAspectRatioBinding.h"
#include "nsSMILValue.h"
#include "nsSVGAttrTearoffTable.h"
#include "nsWhitespaceTokenizer.h"
#include "SMILEnumType.h"
#include "SVGContentUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

////////////////////////////////////////////////////////////////////////
// SVGAnimatedPreserveAspectRatio class
NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(DOMSVGAnimatedPreserveAspectRatio, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMSVGAnimatedPreserveAspectRatio)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMSVGAnimatedPreserveAspectRatio)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMSVGAnimatedPreserveAspectRatio)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
DOMSVGAnimatedPreserveAspectRatio::WrapObject(JSContext* aCx)
{
  return SVGAnimatedPreserveAspectRatioBinding::Wrap(aCx, this);
}

/* Implementation */

static const char *sAlignStrings[] =
  { "none", "xMinYMin", "xMidYMin", "xMaxYMin", "xMinYMid", "xMidYMid",
    "xMaxYMid", "xMinYMax", "xMidYMax", "xMaxYMax" };

static const char *sMeetOrSliceStrings[] = { "meet", "slice" };

static nsSVGAttrTearoffTable<SVGAnimatedPreserveAspectRatio, DOMSVGAnimatedPreserveAspectRatio>
  sSVGAnimatedPAspectRatioTearoffTable;
static nsSVGAttrTearoffTable<SVGAnimatedPreserveAspectRatio, DOMSVGPreserveAspectRatio>
  sBaseSVGPAspectRatioTearoffTable;
static nsSVGAttrTearoffTable<SVGAnimatedPreserveAspectRatio, DOMSVGPreserveAspectRatio>
  sAnimSVGPAspectRatioTearoffTable;

static uint16_t
GetAlignForString(const nsAString &aAlignString)
{
  for (uint32_t i = 0 ; i < ArrayLength(sAlignStrings) ; i++) {
    if (aAlignString.EqualsASCII(sAlignStrings[i])) {
      return (i + SVG_ALIGN_MIN_VALID);
    }
  }

  return SVG_PRESERVEASPECTRATIO_UNKNOWN;
}

static void
GetAlignString(nsAString& aAlignString, uint16_t aAlign)
{
  NS_ASSERTION(
    aAlign >= SVG_ALIGN_MIN_VALID && aAlign <= SVG_ALIGN_MAX_VALID,
    "Unknown align");

  aAlignString.AssignASCII(
    sAlignStrings[aAlign - SVG_ALIGN_MIN_VALID]);
}

static uint16_t
GetMeetOrSliceForString(const nsAString &aMeetOrSlice)
{
  for (uint32_t i = 0 ; i < ArrayLength(sMeetOrSliceStrings) ; i++) {
    if (aMeetOrSlice.EqualsASCII(sMeetOrSliceStrings[i])) {
      return (i + SVG_MEETORSLICE_MIN_VALID);
    }
  }

  return SVG_MEETORSLICE_UNKNOWN;
}

static void
GetMeetOrSliceString(nsAString& aMeetOrSliceString, uint16_t aMeetOrSlice)
{
  NS_ASSERTION(
    aMeetOrSlice >= SVG_MEETORSLICE_MIN_VALID &&
    aMeetOrSlice <= SVG_MEETORSLICE_MAX_VALID,
    "Unknown meetOrSlice");

  aMeetOrSliceString.AssignASCII(
    sMeetOrSliceStrings[aMeetOrSlice - SVG_MEETORSLICE_MIN_VALID]);
}

already_AddRefed<DOMSVGPreserveAspectRatio>
DOMSVGAnimatedPreserveAspectRatio::BaseVal()
{
  nsRefPtr<DOMSVGPreserveAspectRatio> domBaseVal =
    sBaseSVGPAspectRatioTearoffTable.GetTearoff(mVal);
  if (!domBaseVal) {
    domBaseVal = new DOMSVGPreserveAspectRatio(mVal, mSVGElement, true);
    sBaseSVGPAspectRatioTearoffTable.AddTearoff(mVal, domBaseVal);
  }

  return domBaseVal.forget();
}

DOMSVGPreserveAspectRatio::~DOMSVGPreserveAspectRatio()
{
  if (mIsBaseValue) {
    sBaseSVGPAspectRatioTearoffTable.RemoveTearoff(mVal);
  } else {
    sAnimSVGPAspectRatioTearoffTable.RemoveTearoff(mVal);
  }
}

already_AddRefed<DOMSVGPreserveAspectRatio>
DOMSVGAnimatedPreserveAspectRatio::AnimVal()
{
  nsRefPtr<DOMSVGPreserveAspectRatio> domAnimVal =
    sAnimSVGPAspectRatioTearoffTable.GetTearoff(mVal);
  if (!domAnimVal) {
    domAnimVal = new DOMSVGPreserveAspectRatio(mVal, mSVGElement, false);
    sAnimSVGPAspectRatioTearoffTable.AddTearoff(mVal, domAnimVal);
  }

  return domAnimVal.forget();
}

static nsresult
ToPreserveAspectRatio(const nsAString &aString,
                      SVGPreserveAspectRatio *aValue)
{
  nsWhitespaceTokenizerTemplate<IsSVGWhitespace> tokenizer(aString);
  if (tokenizer.whitespaceBeforeFirstToken() ||
      !tokenizer.hasMoreTokens()) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }
  const nsAString &token = tokenizer.nextToken();

  nsresult rv;
  SVGPreserveAspectRatio val;

  val.SetDefer(token.EqualsLiteral("defer"));

  if (val.GetDefer()) {
    if (!tokenizer.hasMoreTokens()) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }
    rv = val.SetAlign(GetAlignForString(tokenizer.nextToken()));
  } else {
    rv = val.SetAlign(GetAlignForString(token));
  }

  if (NS_FAILED(rv)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  if (tokenizer.hasMoreTokens()) {
    rv = val.SetMeetOrSlice(GetMeetOrSliceForString(tokenizer.nextToken()));
    if (NS_FAILED(rv)) {
      return NS_ERROR_DOM_SYNTAX_ERR;
    }
  } else {
    val.SetMeetOrSlice(SVG_MEETORSLICE_MEET);
  }

  if (tokenizer.whitespaceAfterCurrentToken()) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  *aValue = val;
  return NS_OK;
}

nsresult
SVGAnimatedPreserveAspectRatio::SetBaseValueString(
  const nsAString &aValueAsString, nsSVGElement *aSVGElement, bool aDoSetAttr)
{
  SVGPreserveAspectRatio val;
  nsresult res = ToPreserveAspectRatio(aValueAsString, &val);
  if (NS_FAILED(res)) {
    return res;
  }

  nsAttrValue emptyOrOldValue;
  if (aDoSetAttr) {
    emptyOrOldValue = aSVGElement->WillChangePreserveAspectRatio();
  }

  mBaseVal = val;
  mIsBaseSet = true;

  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  }
  if (aDoSetAttr) {
    aSVGElement->DidChangePreserveAspectRatio(emptyOrOldValue);
  }
  if (mIsAnimated) {
    aSVGElement->AnimationNeedsResample();
  }
  return NS_OK;
}

void
SVGAnimatedPreserveAspectRatio::GetBaseValueString(
  nsAString& aValueAsString) const
{
  nsAutoString tmpString;

  aValueAsString.Truncate();

  if (mBaseVal.mDefer) {
    aValueAsString.AppendLiteral("defer ");
  }

  GetAlignString(tmpString, mBaseVal.mAlign);
  aValueAsString.Append(tmpString);

  if (mBaseVal.mAlign != uint8_t(SVG_PRESERVEASPECTRATIO_NONE)) {

    aValueAsString.AppendLiteral(" ");
    GetMeetOrSliceString(tmpString, mBaseVal.mMeetOrSlice);
    aValueAsString.Append(tmpString);
  }
}

void
SVGAnimatedPreserveAspectRatio::SetBaseValue(const SVGPreserveAspectRatio &aValue,
                                             nsSVGElement *aSVGElement)
{
  if (mIsBaseSet && mBaseVal == aValue) {
    return;
  }

  nsAttrValue emptyOrOldValue = aSVGElement->WillChangePreserveAspectRatio();
  mBaseVal = aValue;
  mIsBaseSet = true;

  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  }
  aSVGElement->DidChangePreserveAspectRatio(emptyOrOldValue);
  if (mIsAnimated) {
    aSVGElement->AnimationNeedsResample();
  }
}

static uint64_t
PackPreserveAspectRatio(const SVGPreserveAspectRatio& par)
{
  // All preserveAspectRatio values are enum values (do not interpolate), so we
  // can safely collate them and treat them as a single enum as for SMIL.
  uint64_t packed = 0;
  packed |= uint64_t(par.GetDefer() ? 1 : 0) << 16;
  packed |= uint64_t(par.GetAlign()) << 8;
  packed |= uint64_t(par.GetMeetOrSlice());
  return packed;
}

void
SVGAnimatedPreserveAspectRatio::SetAnimValue(uint64_t aPackedValue,
                                             nsSVGElement *aSVGElement)
{
  if (mIsAnimated && PackPreserveAspectRatio(mAnimVal) == aPackedValue) {
    return;
  }
  mAnimVal.SetDefer(((aPackedValue & 0xff0000) >> 16) ? true : false);
  mAnimVal.SetAlign(uint16_t((aPackedValue & 0xff00) >> 8));
  mAnimVal.SetMeetOrSlice(uint16_t(aPackedValue & 0xff));
  mIsAnimated = true;
  aSVGElement->DidAnimatePreserveAspectRatio();
}

nsresult
SVGAnimatedPreserveAspectRatio::ToDOMAnimatedPreserveAspectRatio(
  DOMSVGAnimatedPreserveAspectRatio **aResult,
  nsSVGElement *aSVGElement)
{
  nsRefPtr<DOMSVGAnimatedPreserveAspectRatio> domAnimatedPAspectRatio =
    sSVGAnimatedPAspectRatioTearoffTable.GetTearoff(this);
  if (!domAnimatedPAspectRatio) {
    domAnimatedPAspectRatio = new DOMSVGAnimatedPreserveAspectRatio(this, aSVGElement);
    sSVGAnimatedPAspectRatioTearoffTable.AddTearoff(this, domAnimatedPAspectRatio);
  }
  domAnimatedPAspectRatio.forget(aResult);
  return NS_OK;
}

DOMSVGAnimatedPreserveAspectRatio::~DOMSVGAnimatedPreserveAspectRatio()
{
  sSVGAnimatedPAspectRatioTearoffTable.RemoveTearoff(mVal);
}

nsISMILAttr*
SVGAnimatedPreserveAspectRatio::ToSMILAttr(nsSVGElement *aSVGElement)
{
  return new SMILPreserveAspectRatio(this, aSVGElement);
}

// typedef for inner class, to make function signatures shorter below:
typedef SVGAnimatedPreserveAspectRatio::SMILPreserveAspectRatio
  SMILPreserveAspectRatio;

nsresult
SMILPreserveAspectRatio::ValueFromString(const nsAString& aStr,
                                         const SVGAnimationElement* /*aSrcElement*/,
                                         nsSMILValue& aValue,
                                         bool& aPreventCachingOfSandwich) const
{
  SVGPreserveAspectRatio par;
  nsresult res = ToPreserveAspectRatio(aStr, &par);
  NS_ENSURE_SUCCESS(res, res);

  nsSMILValue val(SMILEnumType::Singleton());
  val.mU.mUint = PackPreserveAspectRatio(par);
  aValue = val;
  aPreventCachingOfSandwich = false;
  return NS_OK;
}

nsSMILValue
SMILPreserveAspectRatio::GetBaseValue() const
{
  nsSMILValue val(SMILEnumType::Singleton());
  val.mU.mUint = PackPreserveAspectRatio(mVal->GetBaseValue());
  return val;
}

void
SMILPreserveAspectRatio::ClearAnimValue()
{
  if (mVal->mIsAnimated) {
    mVal->mIsAnimated = false;
    mVal->mAnimVal = mVal->mBaseVal;
    mSVGElement->DidAnimatePreserveAspectRatio();
  }
}

nsresult
SMILPreserveAspectRatio::SetAnimValue(const nsSMILValue& aValue)
{
  NS_ASSERTION(aValue.mType == SMILEnumType::Singleton(),
               "Unexpected type to assign animated value");
  if (aValue.mType == SMILEnumType::Singleton()) {
    mVal->SetAnimValue(aValue.mU.mUint, mSVGElement);
  }
  return NS_OK;
}
