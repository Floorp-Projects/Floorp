/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGANGLE_H__
#define __NS_SVGANGLE_H__

#include "nsError.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Attributes.h"
#include "mozilla/SMILAttr.h"
#include "mozilla/dom/SVGAngleBinding.h"
#include "mozilla/UniquePtr.h"

class nsISupports;

namespace mozilla {

class SMILValue;

namespace dom {
class nsSVGOrientType;
class DOMSVGAngle;
class SVGAnimatedAngle;
class SVGAnimationElement;
class SVGElement;
}  // namespace dom

class SVGAngle {
  friend class mozilla::dom::DOMSVGAngle;
  friend class mozilla::dom::SVGAnimatedAngle;
  typedef mozilla::dom::SVGElement SVGElement;

 public:
  void Init(uint8_t aAttrEnum = 0xff, float aValue = 0,
            uint8_t aUnitType =
                mozilla::dom::SVGAngle_Binding::SVG_ANGLETYPE_UNSPECIFIED) {
    mAnimVal = mBaseVal = aValue;
    mAnimValUnit = mBaseValUnit = aUnitType;
    mAttrEnum = aAttrEnum;
    mIsAnimated = false;
  }

  nsresult SetBaseValueString(const nsAString& aValue, SVGElement* aSVGElement,
                              bool aDoSetAttr);
  void GetBaseValueString(nsAString& aValue) const;
  void GetAnimValueString(nsAString& aValue) const;

  float GetBaseValue() const {
    return mBaseVal * GetDegreesPerUnit(mBaseValUnit);
  }
  float GetAnimValue() const {
    return mAnimVal * GetDegreesPerUnit(mAnimValUnit);
  }

  void SetBaseValue(float aValue, uint8_t aUnit, SVGElement* aSVGElement,
                    bool aDoSetAttr);
  void SetAnimValue(float aValue, uint8_t aUnit, SVGElement* aSVGElement);

  uint8_t GetBaseValueUnit() const { return mBaseValUnit; }
  uint8_t GetAnimValueUnit() const { return mAnimValUnit; }
  float GetBaseValInSpecifiedUnits() const { return mBaseVal; }
  float GetAnimValInSpecifiedUnits() const { return mAnimVal; }

  static nsresult ToDOMSVGAngle(nsISupports** aResult);
  already_AddRefed<mozilla::dom::SVGAnimatedAngle> ToDOMAnimatedAngle(
      SVGElement* aSVGElement);
  mozilla::UniquePtr<SMILAttr> ToSMILAttr(SVGElement* aSVGElement);

  static bool GetValueFromString(const nsAString& aString, float& aValue,
                                 uint16_t* aUnitType);
  static float GetDegreesPerUnit(uint8_t aUnit);

 private:
  float mAnimVal;
  float mBaseVal;
  uint8_t mAnimValUnit;
  uint8_t mBaseValUnit;
  uint8_t mAttrEnum;  // element specified tracking for attribute
  bool mIsAnimated;

  void SetBaseValueInSpecifiedUnits(float aValue, SVGElement* aSVGElement);
  nsresult NewValueSpecifiedUnits(uint16_t aUnitType, float aValue,
                                  SVGElement* aSVGElement);
  nsresult ConvertToSpecifiedUnits(uint16_t aUnitType, SVGElement* aSVGElement);
  already_AddRefed<mozilla::dom::DOMSVGAngle> ToDOMBaseVal(
      SVGElement* aSVGElement);
  already_AddRefed<mozilla::dom::DOMSVGAngle> ToDOMAnimVal(
      SVGElement* aSVGElement);

 public:
  // We do not currently implemente a SMILAngle struct because in SVG 1.1 the
  // only *animatable* attribute that takes an <angle> is 'orient', on the
  // 'marker' element, and 'orient' must be special cased since it can also
  // take the value 'auto', making it a more complex type.

  struct SMILOrient final : public SMILAttr {
   public:
    SMILOrient(mozilla::dom::nsSVGOrientType* aOrientType, SVGAngle* aAngle,
               SVGElement* aSVGElement)
        : mOrientType(aOrientType), mAngle(aAngle), mSVGElement(aSVGElement) {}

    // These will stay alive because a SMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    mozilla::dom::nsSVGOrientType* mOrientType;
    SVGAngle* mAngle;
    SVGElement* mSVGElement;

    // SMILAttr methods
    virtual nsresult ValueFromString(
        const nsAString& aStr,
        const mozilla::dom::SVGAnimationElement* aSrcElement, SMILValue& aValue,
        bool& aPreventCachingOfSandwich) const override;
    virtual SMILValue GetBaseValue() const override;
    virtual void ClearAnimValue() override;
    virtual nsresult SetAnimValue(const SMILValue& aValue) override;
  };
};

}  // namespace mozilla

#endif  //__NS_SVGANGLE_H__
