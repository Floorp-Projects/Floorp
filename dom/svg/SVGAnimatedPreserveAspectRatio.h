/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGANIMATEDPRESERVEASPECTRATIO_H__
#define MOZILLA_SVGANIMATEDPRESERVEASPECTRATIO_H__

#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsError.h"
#include "nsISMILAttr.h"
#include "nsSVGElement.h"
#include "SVGPreserveAspectRatio.h"
#include "mozilla/Attributes.h"

class nsSMILValue;

namespace mozilla {
namespace dom {
class DOMSVGAnimatedPreserveAspectRatio;
class SVGAnimationElement;
}

class SVGAnimatedPreserveAspectRatio final
{
public:
  void Init() {
    mBaseVal.mAlign = SVG_PRESERVEASPECTRATIO_XMIDYMID;
    mBaseVal.mMeetOrSlice = SVG_MEETORSLICE_MEET;
    mBaseVal.mDefer = false;
    mAnimVal = mBaseVal;
    mIsAnimated = false;
    mIsBaseSet = false;
  }

  nsresult SetBaseValueString(const nsAString& aValue,
                              nsSVGElement *aSVGElement,
                              bool aDoSetAttr);
  void GetBaseValueString(nsAString& aValue) const;

  void SetBaseValue(const SVGPreserveAspectRatio &aValue,
                    nsSVGElement *aSVGElement);
  nsresult SetBaseAlign(uint16_t aAlign, nsSVGElement *aSVGElement) {
    if (aAlign < SVG_ALIGN_MIN_VALID || aAlign > SVG_ALIGN_MAX_VALID) {
      return NS_ERROR_FAILURE;
    }
    SetBaseValue(SVGPreserveAspectRatio(
                   static_cast<SVGAlign>(aAlign), mBaseVal.GetMeetOrSlice(),
                   mBaseVal.GetDefer()),
                 aSVGElement);
    return NS_OK;
  }
  nsresult SetBaseMeetOrSlice(uint16_t aMeetOrSlice, nsSVGElement *aSVGElement) {
    if (aMeetOrSlice < SVG_MEETORSLICE_MIN_VALID ||
        aMeetOrSlice > SVG_MEETORSLICE_MAX_VALID) {
      return NS_ERROR_FAILURE;
    }
    SetBaseValue(SVGPreserveAspectRatio(
                   mBaseVal.GetAlign(), static_cast<SVGMeetOrSlice>(aMeetOrSlice),
                   mBaseVal.GetDefer()),
                 aSVGElement);
    return NS_OK;
  }
  void SetAnimValue(uint64_t aPackedValue, nsSVGElement *aSVGElement);

  const SVGPreserveAspectRatio &GetBaseValue() const
    { return mBaseVal; }
  const SVGPreserveAspectRatio &GetAnimValue() const
    { return mAnimVal; }
  bool IsAnimated() const
    { return mIsAnimated; }
  bool IsExplicitlySet() const
    { return mIsAnimated || mIsBaseSet; }

  already_AddRefed<mozilla::dom::DOMSVGAnimatedPreserveAspectRatio>
  ToDOMAnimatedPreserveAspectRatio(nsSVGElement* aSVGElement);
  // Returns a new nsISMILAttr object that the caller must delete
  nsISMILAttr* ToSMILAttr(nsSVGElement* aSVGElement);

private:

  SVGPreserveAspectRatio mAnimVal;
  SVGPreserveAspectRatio mBaseVal;
  bool mIsAnimated;
  bool mIsBaseSet;

public:
  struct SMILPreserveAspectRatio final : public nsISMILAttr
  {
  public:
    SMILPreserveAspectRatio(SVGAnimatedPreserveAspectRatio* aVal,
                            nsSVGElement* aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}

    // These will stay alive because a nsISMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    SVGAnimatedPreserveAspectRatio* mVal;
    nsSVGElement* mSVGElement;

    // nsISMILAttr methods
    virtual nsresult ValueFromString(const nsAString& aStr,
                                     const dom::SVGAnimationElement* aSrcElement,
                                     nsSMILValue& aValue,
                                     bool& aPreventCachingOfSandwich) const override;
    virtual nsSMILValue GetBaseValue() const override;
    virtual void ClearAnimValue() override;
    virtual nsresult SetAnimValue(const nsSMILValue& aValue) override;
  };
};

namespace dom {
class DOMSVGAnimatedPreserveAspectRatio final : public nsISupports,
                                                public nsWrapperCache
{
  ~DOMSVGAnimatedPreserveAspectRatio();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMSVGAnimatedPreserveAspectRatio)

  DOMSVGAnimatedPreserveAspectRatio(SVGAnimatedPreserveAspectRatio* aVal,
                                    nsSVGElement *aSVGElement)
    : mVal(aVal), mSVGElement(aSVGElement)
  {
  }

  // WebIDL
  nsSVGElement* GetParentObject() const { return mSVGElement; }
  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  // These aren't weak refs because new objects are returned each time
  already_AddRefed<DOMSVGPreserveAspectRatio> BaseVal();
  already_AddRefed<DOMSVGPreserveAspectRatio> AnimVal();

protected:
  // kept alive because it belongs to content:
  SVGAnimatedPreserveAspectRatio* mVal;
  nsRefPtr<nsSVGElement> mSVGElement;
};

} // namespace dom
} // namespace mozilla

#endif // MOZILLA_SVGANIMATEDPRESERVEASPECTRATIO_H__
