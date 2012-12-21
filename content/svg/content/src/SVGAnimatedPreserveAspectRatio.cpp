/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "SVGAnimatedPreserveAspectRatio.h"
#include "nsWhitespaceTokenizer.h"
#include "nsSMILValue.h"
#include "nsSVGAttrTearoffTable.h"
#include "SMILEnumType.h"
#include "nsAttrValueInlines.h"

using namespace mozilla;

////////////////////////////////////////////////////////////////////////
// SVGAnimatedPreserveAspectRatio class

NS_SVG_VAL_IMPL_CYCLE_COLLECTION(
  SVGAnimatedPreserveAspectRatio::DOMBaseVal, mSVGElement)
NS_SVG_VAL_IMPL_CYCLE_COLLECTION(
  SVGAnimatedPreserveAspectRatio::DOMAnimVal, mSVGElement)
NS_SVG_VAL_IMPL_CYCLE_COLLECTION(
  SVGAnimatedPreserveAspectRatio::DOMAnimPAspectRatio, mSVGElement)

NS_IMPL_CYCLE_COLLECTING_ADDREF(SVGAnimatedPreserveAspectRatio::DOMBaseVal)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SVGAnimatedPreserveAspectRatio::DOMBaseVal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(SVGAnimatedPreserveAspectRatio::DOMAnimVal)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SVGAnimatedPreserveAspectRatio::DOMAnimVal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(
  SVGAnimatedPreserveAspectRatio::DOMAnimPAspectRatio)
NS_IMPL_CYCLE_COLLECTING_RELEASE(
  SVGAnimatedPreserveAspectRatio::DOMAnimPAspectRatio)

DOMCI_DATA(SVGPreserveAspectRatio, SVGAnimatedPreserveAspectRatio::DOMBaseVal)
DOMCI_DATA(SVGAnimatedPreserveAspectRatio,
           SVGAnimatedPreserveAspectRatio::DOMAnimPAspectRatio)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(
  SVGAnimatedPreserveAspectRatio::DOMBaseVal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGPreserveAspectRatio)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGPreserveAspectRatio)
NS_INTERFACE_MAP_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(
  SVGAnimatedPreserveAspectRatio::DOMAnimVal)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGPreserveAspectRatio)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGPreserveAspectRatio)
NS_INTERFACE_MAP_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(
  SVGAnimatedPreserveAspectRatio::DOMAnimPAspectRatio)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGAnimatedPreserveAspectRatio)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGAnimatedPreserveAspectRatio)
NS_INTERFACE_MAP_END

/* Implementation */

static const char *sAlignStrings[] =
  { "none", "xMinYMin", "xMidYMin", "xMaxYMin", "xMinYMid", "xMidYMid",
    "xMaxYMid", "xMinYMax", "xMidYMax", "xMaxYMax" };

static const char *sMeetOrSliceStrings[] = { "meet", "slice" };

static nsSVGAttrTearoffTable<SVGAnimatedPreserveAspectRatio, SVGAnimatedPreserveAspectRatio::DOMAnimPAspectRatio>
  sSVGAnimatedPAspectRatioTearoffTable;
static nsSVGAttrTearoffTable<SVGAnimatedPreserveAspectRatio, SVGAnimatedPreserveAspectRatio::DOMBaseVal>
  sBaseSVGPAspectRatioTearoffTable;
static nsSVGAttrTearoffTable<SVGAnimatedPreserveAspectRatio, SVGAnimatedPreserveAspectRatio::DOMAnimVal>
  sAnimSVGPAspectRatioTearoffTable;

static uint16_t
GetAlignForString(const nsAString &aAlignString)
{
  for (uint32_t i = 0 ; i < ArrayLength(sAlignStrings) ; i++) {
    if (aAlignString.EqualsASCII(sAlignStrings[i])) {
      return (i + nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE);
    }
  }

  return nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_UNKNOWN;
}

static void
GetAlignString(nsAString& aAlignString, uint16_t aAlign)
{
  NS_ASSERTION(
    aAlign >= nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE &&
    aAlign <= nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX,
    "Unknown align");

  aAlignString.AssignASCII(
    sAlignStrings[aAlign -
                  nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE]);
}

static uint16_t
GetMeetOrSliceForString(const nsAString &aMeetOrSlice)
{
  for (uint32_t i = 0 ; i < ArrayLength(sMeetOrSliceStrings) ; i++) {
    if (aMeetOrSlice.EqualsASCII(sMeetOrSliceStrings[i])) {
      return (i + nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET);
    }
  }

  return nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_UNKNOWN;
}

static void
GetMeetOrSliceString(nsAString& aMeetOrSliceString, uint16_t aMeetOrSlice)
{
  NS_ASSERTION(
    aMeetOrSlice >= nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET &&
    aMeetOrSlice <= nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_SLICE,
    "Unknown meetOrSlice");

  aMeetOrSliceString.AssignASCII(
    sMeetOrSliceStrings[aMeetOrSlice -
                        nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET]);
}

bool
SVGPreserveAspectRatio::operator==(const SVGPreserveAspectRatio& aOther) const
{
  return mAlign == aOther.mAlign &&
    mMeetOrSlice == aOther.mMeetOrSlice &&
    mDefer == aOther.mDefer;
}

nsresult
SVGAnimatedPreserveAspectRatio::ToDOMBaseVal(
  nsIDOMSVGPreserveAspectRatio **aResult,
  nsSVGElement *aSVGElement)
{
  nsRefPtr<DOMBaseVal> domBaseVal =
    sBaseSVGPAspectRatioTearoffTable.GetTearoff(this);
  if (!domBaseVal) {
    domBaseVal = new DOMBaseVal(this, aSVGElement);
    sBaseSVGPAspectRatioTearoffTable.AddTearoff(this, domBaseVal);
  }

  domBaseVal.forget(aResult);
  return NS_OK;
}

SVGAnimatedPreserveAspectRatio::DOMBaseVal::~DOMBaseVal()
{
  sBaseSVGPAspectRatioTearoffTable.RemoveTearoff(mVal);
}

nsresult
SVGAnimatedPreserveAspectRatio::ToDOMAnimVal(
  nsIDOMSVGPreserveAspectRatio **aResult,
  nsSVGElement *aSVGElement)
{
  nsRefPtr<DOMAnimVal> domAnimVal =
    sAnimSVGPAspectRatioTearoffTable.GetTearoff(this);
  if (!domAnimVal) {
    domAnimVal = new DOMAnimVal(this, aSVGElement);
    sAnimSVGPAspectRatioTearoffTable.AddTearoff(this, domAnimVal);
  }

  domAnimVal.forget(aResult);
  return NS_OK;
}

SVGAnimatedPreserveAspectRatio::DOMAnimVal::~DOMAnimVal()
{
  sAnimSVGPAspectRatioTearoffTable.RemoveTearoff(mVal);
}

static nsresult
ToPreserveAspectRatio(const nsAString &aString,
                      SVGPreserveAspectRatio *aValue)
{
  if (aString.IsEmpty() || NS_IsAsciiWhitespace(aString[0])) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  nsWhitespaceTokenizer tokenizer(aString);
  if (!tokenizer.hasMoreTokens()) {
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
    val.SetMeetOrSlice(nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET);
  }

  if (tokenizer.hasMoreTokens()) {
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

  if (mBaseVal.mAlign !=
      nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE) {

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
  nsIDOMSVGAnimatedPreserveAspectRatio **aResult,
  nsSVGElement *aSVGElement)
{
  nsRefPtr<DOMAnimPAspectRatio> domAnimatedPAspectRatio =
    sSVGAnimatedPAspectRatioTearoffTable.GetTearoff(this);
  if (!domAnimatedPAspectRatio) {
    domAnimatedPAspectRatio = new DOMAnimPAspectRatio(this, aSVGElement);
    sSVGAnimatedPAspectRatioTearoffTable.AddTearoff(this, domAnimatedPAspectRatio);
  }
  domAnimatedPAspectRatio.forget(aResult);
  return NS_OK;
}

SVGAnimatedPreserveAspectRatio::DOMAnimPAspectRatio::~DOMAnimPAspectRatio()
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
                                         const nsISMILAnimationElement* /*aSrcElement*/,
                                         nsSMILValue& aValue,
                                         bool& aPreventCachingOfSandwich) const
{
  SVGPreserveAspectRatio par;
  nsresult res = ToPreserveAspectRatio(aStr, &par);
  NS_ENSURE_SUCCESS(res, res);

  nsSMILValue val(&SMILEnumType::sSingleton);
  val.mU.mUint = PackPreserveAspectRatio(par);
  aValue = val;
  aPreventCachingOfSandwich = false;
  return NS_OK;
}

nsSMILValue
SMILPreserveAspectRatio::GetBaseValue() const
{
  nsSMILValue val(&SMILEnumType::sSingleton);
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
  NS_ASSERTION(aValue.mType == &SMILEnumType::sSingleton,
               "Unexpected type to assign animated value");
  if (aValue.mType == &SMILEnumType::sSingleton) {
    mVal->SetAnimValue(aValue.mU.mUint, mSVGElement);
  }
  return NS_OK;
}
