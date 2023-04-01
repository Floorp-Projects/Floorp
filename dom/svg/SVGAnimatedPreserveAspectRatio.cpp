/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGAnimatedPreserveAspectRatio.h"

#include "mozAutoDocUpdate.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Maybe.h"
#include "mozilla/SMILValue.h"
#include "mozilla/SVGContentUtils.h"
#include "mozilla/dom/SVGAnimatedPreserveAspectRatioBinding.h"
#include "SMILEnumType.h"
#include "SVGAttrTearoffTable.h"

using namespace mozilla::dom;

namespace mozilla {

////////////////////////////////////////////////////////////////////////
// SVGAnimatedPreserveAspectRatio class
NS_SVG_VAL_IMPL_CYCLE_COLLECTION_WRAPPERCACHED(
    DOMSVGAnimatedPreserveAspectRatio, mSVGElement)

JSObject* DOMSVGAnimatedPreserveAspectRatio::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return SVGAnimatedPreserveAspectRatio_Binding::Wrap(aCx, this, aGivenProto);
}

/* Implementation */

//----------------------------------------------------------------------
// Helper class: AutoChangePreserveAspectRatioNotifier
// Stack-based helper class to pair calls to WillChangePreserveAspectRatio and
// DidChangePreserveAspectRatio.
class MOZ_RAII AutoChangePreserveAspectRatioNotifier {
 public:
  AutoChangePreserveAspectRatioNotifier(
      SVGAnimatedPreserveAspectRatio* aPreserveAspectRatio,
      SVGElement* aSVGElement, bool aDoSetAttr = true)
      : mPreserveAspectRatio(aPreserveAspectRatio),
        mSVGElement(aSVGElement),
        mDoSetAttr(aDoSetAttr) {
    MOZ_ASSERT(mPreserveAspectRatio, "Expecting non-null preserveAspectRatio");
    MOZ_ASSERT(mSVGElement, "Expecting non-null element");
    if (mDoSetAttr) {
      mUpdateBatch.emplace(aSVGElement->GetComposedDoc(), true);
      mEmptyOrOldValue =
          mSVGElement->WillChangePreserveAspectRatio(mUpdateBatch.ref());
    }
  }

  ~AutoChangePreserveAspectRatioNotifier() {
    if (mDoSetAttr) {
      mSVGElement->DidChangePreserveAspectRatio(mEmptyOrOldValue,
                                                mUpdateBatch.ref());
    }
    if (mPreserveAspectRatio->mIsAnimated) {
      mSVGElement->AnimationNeedsResample();
    }
  }

 private:
  SVGAnimatedPreserveAspectRatio* const mPreserveAspectRatio;
  SVGElement* const mSVGElement;
  Maybe<mozAutoDocUpdate> mUpdateBatch;
  nsAttrValue mEmptyOrOldValue;
  bool mDoSetAttr;
};

static SVGAttrTearoffTable<SVGAnimatedPreserveAspectRatio,
                           DOMSVGAnimatedPreserveAspectRatio>
    sSVGAnimatedPAspectRatioTearoffTable;
static SVGAttrTearoffTable<SVGAnimatedPreserveAspectRatio,
                           DOMSVGPreserveAspectRatio>
    sBaseSVGPAspectRatioTearoffTable;
static SVGAttrTearoffTable<SVGAnimatedPreserveAspectRatio,
                           DOMSVGPreserveAspectRatio>
    sAnimSVGPAspectRatioTearoffTable;

already_AddRefed<DOMSVGPreserveAspectRatio>
DOMSVGAnimatedPreserveAspectRatio::BaseVal() {
  RefPtr<DOMSVGPreserveAspectRatio> domBaseVal =
      sBaseSVGPAspectRatioTearoffTable.GetTearoff(mVal);
  if (!domBaseVal) {
    domBaseVal = new DOMSVGPreserveAspectRatio(mVal, mSVGElement, true);
    sBaseSVGPAspectRatioTearoffTable.AddTearoff(mVal, domBaseVal);
  }

  return domBaseVal.forget();
}

DOMSVGPreserveAspectRatio::~DOMSVGPreserveAspectRatio() {
  if (mIsBaseValue) {
    sBaseSVGPAspectRatioTearoffTable.RemoveTearoff(mVal);
  } else {
    sAnimSVGPAspectRatioTearoffTable.RemoveTearoff(mVal);
  }
}

already_AddRefed<DOMSVGPreserveAspectRatio>
DOMSVGAnimatedPreserveAspectRatio::AnimVal() {
  RefPtr<DOMSVGPreserveAspectRatio> domAnimVal =
      sAnimSVGPAspectRatioTearoffTable.GetTearoff(mVal);
  if (!domAnimVal) {
    domAnimVal = new DOMSVGPreserveAspectRatio(mVal, mSVGElement, false);
    sAnimSVGPAspectRatioTearoffTable.AddTearoff(mVal, domAnimVal);
  }

  return domAnimVal.forget();
}

nsresult SVGAnimatedPreserveAspectRatio::SetBaseValueString(
    const nsAString& aValueAsString, SVGElement* aSVGElement, bool aDoSetAttr) {
  SVGPreserveAspectRatio val;
  nsresult res = SVGPreserveAspectRatio::FromString(aValueAsString, &val);
  if (NS_FAILED(res)) {
    return res;
  }

  AutoChangePreserveAspectRatioNotifier notifier(this, aSVGElement, aDoSetAttr);

  mBaseVal = val;
  mIsBaseSet = true;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  }

  return NS_OK;
}

void SVGAnimatedPreserveAspectRatio::GetBaseValueString(
    nsAString& aValueAsString) const {
  mBaseVal.ToString(aValueAsString);
}

void SVGAnimatedPreserveAspectRatio::SetBaseValue(
    const SVGPreserveAspectRatio& aValue, SVGElement* aSVGElement) {
  if (mIsBaseSet && mBaseVal == aValue) {
    return;
  }

  AutoChangePreserveAspectRatioNotifier notifier(this, aSVGElement);

  mBaseVal = aValue;
  mIsBaseSet = true;
  if (!mIsAnimated) {
    mAnimVal = mBaseVal;
  }
}

static uint64_t PackPreserveAspectRatio(const SVGPreserveAspectRatio& par) {
  // All preserveAspectRatio values are enum values (do not interpolate), so we
  // can safely collate them and treat them as a single enum as for SMIL.
  uint64_t packed = 0;
  packed |= uint64_t(par.GetAlign()) << 8;
  packed |= uint64_t(par.GetMeetOrSlice());
  return packed;
}

void SVGAnimatedPreserveAspectRatio::SetAnimValue(uint64_t aPackedValue,
                                                  SVGElement* aSVGElement) {
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
    SVGElement* aSVGElement) {
  RefPtr<DOMSVGAnimatedPreserveAspectRatio> domAnimatedPAspectRatio =
      sSVGAnimatedPAspectRatioTearoffTable.GetTearoff(this);
  if (!domAnimatedPAspectRatio) {
    domAnimatedPAspectRatio =
        new DOMSVGAnimatedPreserveAspectRatio(this, aSVGElement);
    sSVGAnimatedPAspectRatioTearoffTable.AddTearoff(this,
                                                    domAnimatedPAspectRatio);
  }
  return domAnimatedPAspectRatio.forget();
}

DOMSVGAnimatedPreserveAspectRatio::~DOMSVGAnimatedPreserveAspectRatio() {
  sSVGAnimatedPAspectRatioTearoffTable.RemoveTearoff(mVal);
}

UniquePtr<SMILAttr> SVGAnimatedPreserveAspectRatio::ToSMILAttr(
    SVGElement* aSVGElement) {
  return MakeUnique<SMILPreserveAspectRatio>(this, aSVGElement);
}

// typedef for inner class, to make function signatures shorter below:
using SMILPreserveAspectRatio =
    SVGAnimatedPreserveAspectRatio::SMILPreserveAspectRatio;

nsresult SMILPreserveAspectRatio::ValueFromString(
    const nsAString& aStr, const SVGAnimationElement* /*aSrcElement*/,
    SMILValue& aValue, bool& aPreventCachingOfSandwich) const {
  SVGPreserveAspectRatio par;
  nsresult res = SVGPreserveAspectRatio::FromString(aStr, &par);
  NS_ENSURE_SUCCESS(res, res);

  SMILValue val(SMILEnumType::Singleton());
  val.mU.mUint = PackPreserveAspectRatio(par);
  aValue = val;
  return NS_OK;
}

SMILValue SMILPreserveAspectRatio::GetBaseValue() const {
  SMILValue val(SMILEnumType::Singleton());
  val.mU.mUint = PackPreserveAspectRatio(mVal->GetBaseValue());
  return val;
}

void SMILPreserveAspectRatio::ClearAnimValue() {
  if (mVal->mIsAnimated) {
    mVal->mIsAnimated = false;
    mVal->mAnimVal = mVal->mBaseVal;
    mSVGElement->DidAnimatePreserveAspectRatio();
  }
}

nsresult SMILPreserveAspectRatio::SetAnimValue(const SMILValue& aValue) {
  NS_ASSERTION(aValue.mType == SMILEnumType::Singleton(),
               "Unexpected type to assign animated value");
  if (aValue.mType == SMILEnumType::Singleton()) {
    mVal->SetAnimValue(aValue.mU.mUint, mSVGElement);
  }
  return NS_OK;
}

}  // namespace mozilla
