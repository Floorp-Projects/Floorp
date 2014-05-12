/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGLENGTH2_H__
#define __NS_SVGLENGTH2_H__

#include "mozilla/Attributes.h"
#include "nsAutoPtr.h"
#include "nsCoord.h"
#include "nsCycleCollectionParticipant.h"
#include "nsError.h"
#include "nsIDOMSVGLength.h"
#include "nsISMILAttr.h"
#include "nsMathUtils.h"
#include "nsSVGElement.h"
#include "SVGContentUtils.h"

class nsIFrame;
class nsSMILValue;

namespace mozilla {
class DOMSVGLength;
namespace dom {
class SVGAnimatedLength;
class SVGAnimationElement;
class SVGSVGElement;
}
}

class nsSVGLength2
{
  friend class mozilla::dom::SVGAnimatedLength;
  friend class mozilla::DOMSVGLength;
public:
  void Init(uint8_t aCtxType = SVGContentUtils::XY,
            uint8_t aAttrEnum = 0xff,
            float aValue = 0,
            uint8_t aUnitType = nsIDOMSVGLength::SVG_LENGTHTYPE_NUMBER) {
    mAnimVal = mBaseVal = aValue;
    mSpecifiedUnitType = aUnitType;
    mAttrEnum = aAttrEnum;
    mCtxType = aCtxType;
    mIsAnimated = false;
    mIsBaseSet = false;
  }

  nsSVGLength2& operator=(const nsSVGLength2& aLength) {
    mBaseVal = aLength.mBaseVal;
    mAnimVal = aLength.mAnimVal;
    mSpecifiedUnitType = aLength.mSpecifiedUnitType;
    mIsAnimated = aLength.mIsAnimated;
    mIsBaseSet = aLength.mIsBaseSet;
    return *this;
  }

  nsresult SetBaseValueString(const nsAString& aValue,
                              nsSVGElement *aSVGElement,
                              bool aDoSetAttr);
  void GetBaseValueString(nsAString& aValue) const;
  void GetAnimValueString(nsAString& aValue) const;

  float GetBaseValue(nsSVGElement* aSVGElement) const
    { return mBaseVal / GetUnitScaleFactor(aSVGElement, mSpecifiedUnitType); }
  float GetAnimValue(nsSVGElement* aSVGElement) const
    { return mAnimVal / GetUnitScaleFactor(aSVGElement, mSpecifiedUnitType); }
  float GetAnimValue(nsIFrame* aFrame) const
    { return mAnimVal / GetUnitScaleFactor(aFrame, mSpecifiedUnitType); }

  uint8_t GetCtxType() const { return mCtxType; }
  uint8_t GetSpecifiedUnitType() const { return mSpecifiedUnitType; }
  bool IsPercentage() const
    { return mSpecifiedUnitType == nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE; }
  float GetAnimValInSpecifiedUnits() const { return mAnimVal; }
  float GetBaseValInSpecifiedUnits() const { return mBaseVal; }

  float GetBaseValue(mozilla::dom::SVGSVGElement* aCtx) const
    { return mBaseVal / GetUnitScaleFactor(aCtx, mSpecifiedUnitType); }
  float GetAnimValue(mozilla::dom::SVGSVGElement* aCtx) const
    { return mAnimVal / GetUnitScaleFactor(aCtx, mSpecifiedUnitType); }

  bool HasBaseVal() const {
    return mIsBaseSet;
  }
  // Returns true if the animated value of this length has been explicitly
  // set (either by animation, or by taking on the base value which has been
  // explicitly set by markup or a DOM call), false otherwise.
  // If this returns false, the animated value is still valid, that is,
  // useable, and represents the default base value of the attribute.
  bool IsExplicitlySet() const
    { return mIsAnimated || mIsBaseSet; }

  already_AddRefed<mozilla::dom::SVGAnimatedLength>
  ToDOMAnimatedLength(nsSVGElement* aSVGElement);

  // Returns a new nsISMILAttr object that the caller must delete
  nsISMILAttr* ToSMILAttr(nsSVGElement* aSVGElement);

private:
  
  float mAnimVal;
  float mBaseVal;
  uint8_t mSpecifiedUnitType;
  uint8_t mAttrEnum; // element specified tracking for attribute
  uint8_t mCtxType; // X, Y or Unspecified
  bool mIsAnimated:1;
  bool mIsBaseSet:1;
  
  static float GetMMPerPixel() { return MM_PER_INCH_FLOAT / 96; }
  float GetAxisLength(nsIFrame *aNonSVGFrame) const;
  static float GetEmLength(nsIFrame *aFrame)
    { return SVGContentUtils::GetFontSize(aFrame); }
  static float GetExLength(nsIFrame *aFrame)
    { return SVGContentUtils::GetFontXHeight(aFrame); }
  float GetUnitScaleFactor(nsIFrame *aFrame, uint8_t aUnitType) const;

  float GetMMPerPixel(mozilla::dom::SVGSVGElement *aCtx) const;
  float GetAxisLength(mozilla::dom::SVGSVGElement *aCtx) const;
  static float GetEmLength(nsSVGElement *aSVGElement)
    { return SVGContentUtils::GetFontSize(aSVGElement); }
  static float GetExLength(nsSVGElement *aSVGElement)
    { return SVGContentUtils::GetFontXHeight(aSVGElement); }
  float GetUnitScaleFactor(nsSVGElement *aSVGElement, uint8_t aUnitType) const;
  float GetUnitScaleFactor(mozilla::dom::SVGSVGElement *aCtx, uint8_t aUnitType) const;

  // SetBaseValue and SetAnimValue set the value in user units
  void SetBaseValue(float aValue, nsSVGElement *aSVGElement, bool aDoSetAttr);
  void SetBaseValueInSpecifiedUnits(float aValue, nsSVGElement *aSVGElement,
                                    bool aDoSetAttr);
  void SetAnimValue(float aValue, nsSVGElement *aSVGElement);
  void SetAnimValueInSpecifiedUnits(float aValue, nsSVGElement *aSVGElement);
  nsresult NewValueSpecifiedUnits(uint16_t aUnitType, float aValue,
                                  nsSVGElement *aSVGElement);
  nsresult ConvertToSpecifiedUnits(uint16_t aUnitType, nsSVGElement *aSVGElement);
  nsresult ToDOMBaseVal(mozilla::DOMSVGLength **aResult, nsSVGElement* aSVGElement);
  nsresult ToDOMAnimVal(mozilla::DOMSVGLength **aResult, nsSVGElement* aSVGElement);

public:
  struct SMILLength : public nsISMILAttr
  {
  public:
    SMILLength(nsSVGLength2* aVal, nsSVGElement *aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}

    // These will stay alive because a nsISMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    nsSVGLength2* mVal;
    nsSVGElement* mSVGElement;

    // nsISMILAttr methods
    virtual nsresult ValueFromString(const nsAString& aStr,
                                     const mozilla::dom::SVGAnimationElement* aSrcElement,
                                     nsSMILValue &aValue,
                                     bool& aPreventCachingOfSandwich) const MOZ_OVERRIDE;
    virtual nsSMILValue GetBaseValue() const MOZ_OVERRIDE;
    virtual void ClearAnimValue() MOZ_OVERRIDE;
    virtual nsresult SetAnimValue(const nsSMILValue& aValue) MOZ_OVERRIDE;
  };
};

#endif //  __NS_SVGLENGTH2_H__
