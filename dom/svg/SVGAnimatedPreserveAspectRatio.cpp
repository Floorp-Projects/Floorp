/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "SVGAnimatedPreserveAspectRatio.h"
#include "mozilla/dom/SVGAnimatedPreserveAspectRatioBinding.h"
#include "nsSMILValue.h"
#include "nsSVGAttrTearoffTable.h"
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
DOMSVGAnimatedPreserveAspectRatio::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SVGAnimatedPreserveAspectRatioBinding::Wrap(aCx, this, aGivenProto);
}

/* Implementation */

static nsSVGAttrTearoffTable<SVGAnimatedPreserveAspectRatio, DOMSVGAnimatedPreserveAspectRatio>
  sSVGAnimatedPAspectRatioTearoffTable;
static nsSVGAttrTearoffTable<SVGAnimatedPreserveAspectRatio, DOMSVGPreserveAspectRatio>
  sBaseSVGPAspectRatioTearoffTable;
static nsSVGAttrTearoffTable<SVGAnimatedPreserveAspectRatio, DOMSVGPreserveAspectRatio>
  sAnimSVGPAspectRatioTearoffTable;

already_AddRefed<DOMSVGPreserveAspectRatio>
DOMSVGAnimatedPreserveAspectRatio::BaseVal()
{
  RefPtr<DOMSVGPreserveAspectRatio> domBaseVal =
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
  RefPtr<DOMSVGPreserveAspectRatio> domAnimVal =
    sAnimSVGPAspectRatioTearoffTable.GetTearoff(mVal);
  if (!domAnimVal) {
    domAnimVal = new DOMSVGPreserveAspectRatio(mVal, mSVGElement, false);
    sAnimSVGPAspectRatioTearoffTable.AddTearoff(mVal, domAnimVal);
  }

  return domAnimVal.forget();
}

nsresult
SVGAnimatedPreserveAspectRatio::SetBaseValueString(
  const nsAString &aValueAsString, nsSVGElement *aSVGElement, bool aDoSetAttr)
{
  SVGPreserveAspectRatio val;
  nsresult res = SVGPreserveAspectRatio::FromString(aValueAsString, &val);
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
  mBaseVal.ToString(aValueAsString);
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
  mAnimVal.SetAlign(uint16_t((aPackedValue & 0xff00) >> 8));
  mAnimVal.SetMeetOrSlice(uint16_t(aPackedValue & 0xff));
  mIsAnimated = true;
  aSVGElement->DidAnimatePreserveAspectRatio();
}

already_AddRefed<DOMSVGAnimatedPreserveAspectRatio>
SVGAnimatedPreserveAspectRatio::ToDOMAnimatedPreserveAspectRatio(
  nsSVGElement *aSVGElement)
{
  RefPtr<DOMSVGAnimatedPreserveAspectRatio> domAnimatedPAspectRatio =
    sSVGAnimatedPAspectRatioTearoffTable.GetTearoff(this);
  if (!domAnimatedPAspectRatio) {
    domAnimatedPAspectRatio = new DOMSVGAnimatedPreserveAspectRatio(this, aSVGElement);
    sSVGAnimatedPAspectRatioTearoffTable.AddTearoff(this, domAnimatedPAspectRatio);
  }
  return domAnimatedPAspectRatio.forget();
}

DOMSVGAnimatedPreserveAspectRatio::~DOMSVGAnimatedPreserveAspectRatio()
{
  sSVGAnimatedPAspectRatioTearoffTable.RemoveTearoff(mVal);
}

UniquePtr<nsISMILAttr>
SVGAnimatedPreserveAspectRatio::ToSMILAttr(nsSVGElement *aSVGElement)
{
  return MakeUnique<SMILPreserveAspectRatio>(this, aSVGElement);
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
  nsresult res = SVGPreserveAspectRatio::FromString(aStr, &par);
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
