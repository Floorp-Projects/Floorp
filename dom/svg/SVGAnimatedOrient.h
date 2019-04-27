/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGORIENT_H__
#define __NS_SVGORIENT_H__

#include "DOMSVGAnimatedEnumeration.h"
#include "nsError.h"
#include "SVGAnimatedEnumeration.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Attributes.h"
#include "mozilla/SMILAttr.h"
#include "mozilla/dom/SVGAngleBinding.h"
#include "mozilla/dom/SVGMarkerElementBinding.h"
#include "mozilla/UniquePtr.h"

class nsISupports;

namespace mozilla {

class SMILValue;

namespace dom {
class DOMSVGAngle;
class DOMSVGAnimatedAngle;
class SVGAnimationElement;
class SVGElement;
}  // namespace dom

class SVGAnimatedOrient {
  friend class AutoChangeOrientNotifier;
  friend class mozilla::dom::DOMSVGAngle;
  friend class mozilla::dom::DOMSVGAnimatedAngle;
  typedef mozilla::dom::SVGElement SVGElement;

 public:
  void Init() {
    mAnimVal = mBaseVal = .0f;
    mAnimType = mBaseType =
        dom::SVGMarkerElement_Binding::SVG_MARKER_ORIENT_ANGLE;
    mAnimValUnit = mBaseValUnit =
        dom::SVGAngle_Binding::SVG_ANGLETYPE_UNSPECIFIED;
    mIsAnimated = false;
  }

  nsresult SetBaseValueString(const nsAString& aValue, SVGElement* aSVGElement,
                              bool aDoSetAttr);
  void GetBaseValueString(nsAString& aValue) const;
  void GetBaseAngleValueString(nsAString& aValue) const;
  void GetAnimAngleValueString(nsAString& aValue) const;

  float GetBaseValue() const {
    return mBaseVal * GetDegreesPerUnit(mBaseValUnit);
  }
  float GetAnimValue() const {
    return mAnimVal * GetDegreesPerUnit(mAnimValUnit);
  }
  SVGEnumValue GetAnimType() const { return mAnimType; }

  void SetBaseValue(float aValue, uint8_t aUnit, SVGElement* aSVGElement,
                    bool aDoSetAttr);
  nsresult SetBaseType(SVGEnumValue aValue, SVGElement* aSVGElement);
  void SetAnimValue(float aValue, uint8_t aUnit, SVGElement* aSVGElement);
  void SetAnimType(SVGEnumValue aValue, SVGElement* aSVGElement);

  uint8_t GetBaseValueUnit() const { return mBaseValUnit; }
  uint8_t GetAnimValueUnit() const { return mAnimValUnit; }
  float GetBaseValInSpecifiedUnits() const { return mBaseVal; }
  float GetAnimValInSpecifiedUnits() const { return mAnimVal; }

  static nsresult ToDOMSVGAngle(nsISupports** aResult);
  already_AddRefed<dom::DOMSVGAnimatedAngle> ToDOMAnimatedAngle(
      SVGElement* aSVGElement);
  already_AddRefed<dom::DOMSVGAnimatedEnumeration> ToDOMAnimatedEnum(
      SVGElement* aSVGElement);
  UniquePtr<SMILAttr> ToSMILAttr(SVGElement* aSVGElement);

  static bool GetValueFromString(const nsAString& aString, float& aValue,
                                 uint16_t* aUnitType);
  static float GetDegreesPerUnit(uint8_t aUnit);

 private:
  float mAnimVal;
  float mBaseVal;
  uint8_t mAnimType;
  uint8_t mBaseType;
  uint8_t mAnimValUnit;
  uint8_t mBaseValUnit;
  bool mIsAnimated;

  void SetBaseValueInSpecifiedUnits(float aValue, SVGElement* aSVGElement);
  nsresult NewValueSpecifiedUnits(uint16_t aUnitType,
                                  float aValueInSpecifiedUnits,
                                  SVGElement* aSVGElement);
  nsresult ConvertToSpecifiedUnits(uint16_t aUnitType, SVGElement* aSVGElement);
  already_AddRefed<dom::DOMSVGAngle> ToDOMBaseVal(SVGElement* aSVGElement);
  already_AddRefed<dom::DOMSVGAngle> ToDOMAnimVal(SVGElement* aSVGElement);

 public:
  // DOM wrapper class for the (DOM)SVGAnimatedEnumeration interface where the
  // wrapped class is SVGAnimatedOrient.
  struct DOMAnimatedEnum final : public dom::DOMSVGAnimatedEnumeration {
    DOMAnimatedEnum(SVGAnimatedOrient* aVal, SVGElement* aSVGElement)
        : DOMSVGAnimatedEnumeration(aSVGElement), mVal(aVal) {}
    ~DOMAnimatedEnum();

    SVGAnimatedOrient* mVal;  // kept alive because it belongs to content

    using mozilla::dom::DOMSVGAnimatedEnumeration::SetBaseVal;
    uint16_t BaseVal() override { return Sanitize(mVal->mBaseType); }
    void SetBaseVal(uint16_t aBaseVal, ErrorResult& aRv) override {
      aRv = mVal->SetBaseType(aBaseVal, mSVGElement);
    }
    uint16_t AnimVal() override {
      // Script may have modified animation parameters or timeline -- DOM
      // getters need to flush any resample requests to reflect these
      // modifications.
      mSVGElement->FlushAnimations();
      return Sanitize(mVal->mAnimType);
    }

   private:
    uint16_t Sanitize(uint16_t aValue);
  };

  struct SMILOrient final : public SMILAttr {
   public:
    SMILOrient(SVGAnimatedOrient* aOrient, SVGElement* aSVGElement)
        : mOrient(aOrient), mSVGElement(aSVGElement) {}

    // These will stay alive because a SMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    SVGAnimatedOrient* mOrient;
    SVGElement* mSVGElement;

    // SMILAttr methods
    virtual nsresult ValueFromString(
        const nsAString& aStr, const dom::SVGAnimationElement* aSrcElement,
        SMILValue& aValue, bool& aPreventCachingOfSandwich) const override;
    virtual SMILValue GetBaseValue() const override;
    virtual void ClearAnimValue() override;
    virtual nsresult SetAnimValue(const SMILValue& aValue) override;
  };
};

}  // namespace mozilla

#endif  //__NS_SVGORIENT_H__
