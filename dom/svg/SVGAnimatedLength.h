/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SVGAnimatedLength_h
#define mozilla_SVGAnimatedLength_h

#include "mozilla/Attributes.h"
#include "mozilla/SMILAttr.h"
#include "mozilla/SVGContentUtils.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/SVGLengthBinding.h"
#include "mozilla/dom/SVGElement.h"
#include "mozilla/gfx/Rect.h"
#include "nsCoord.h"
#include "nsCycleCollectionParticipant.h"
#include "nsError.h"
#include "nsMathUtils.h"

class nsIFrame;

namespace mozilla {

class SMILValue;

namespace dom {
class DOMSVGAnimatedLength;
class DOMSVGLength;
class SVGAnimationElement;
class SVGViewportElement;

class UserSpaceMetrics {
 public:
  virtual ~UserSpaceMetrics() = default;

  virtual float GetEmLength() const = 0;
  virtual float GetExLength() const = 0;
  virtual float GetAxisLength(uint8_t aCtxType) const = 0;
};

class UserSpaceMetricsWithSize : public UserSpaceMetrics {
 public:
  virtual gfx::Size GetSize() const = 0;
  virtual float GetAxisLength(uint8_t aCtxType) const override;
};

class SVGElementMetrics : public UserSpaceMetrics {
 public:
  typedef mozilla::dom::SVGElement SVGElement;
  typedef mozilla::dom::SVGViewportElement SVGViewportElement;

  explicit SVGElementMetrics(SVGElement* aSVGElement,
                             SVGViewportElement* aCtx = nullptr);

  virtual float GetEmLength() const override;
  virtual float GetExLength() const override;
  virtual float GetAxisLength(uint8_t aCtxType) const override;

 private:
  bool EnsureCtx() const;

  SVGElement* mSVGElement;
  mutable SVGViewportElement* mCtx;
};

class NonSVGFrameUserSpaceMetrics : public UserSpaceMetricsWithSize {
 public:
  explicit NonSVGFrameUserSpaceMetrics(nsIFrame* aFrame);

  virtual float GetEmLength() const override;
  virtual float GetExLength() const override;
  virtual gfx::Size GetSize() const override;

 private:
  nsIFrame* mFrame;
};

}  // namespace dom

class SVGAnimatedLength {
  friend class mozilla::dom::DOMSVGAnimatedLength;
  friend class mozilla::dom::DOMSVGLength;
  typedef mozilla::dom::DOMSVGLength DOMSVGLength;
  typedef mozilla::dom::SVGElement SVGElement;
  typedef mozilla::dom::SVGViewportElement SVGViewportElement;
  typedef mozilla::dom::UserSpaceMetrics UserSpaceMetrics;
  typedef mozilla::SMILAttr SMILAttr;
  typedef mozilla::SMILValue SMILValue;
  typedef mozilla::SVGContentUtils SVGContentUtils;

 public:
  void Init(uint8_t aCtxType = mozilla::SVGContentUtils::XY,
            uint8_t aAttrEnum = 0xff, float aValue = 0,
            uint8_t aUnitType =
                mozilla::dom::SVGLength_Binding::SVG_LENGTHTYPE_NUMBER) {
    mAnimVal = mBaseVal = aValue;
    mSpecifiedUnitType = aUnitType;
    mAttrEnum = aAttrEnum;
    mCtxType = aCtxType;
    mIsAnimated = false;
    mIsBaseSet = false;
  }

  SVGAnimatedLength& operator=(const SVGAnimatedLength& aLength) {
    mBaseVal = aLength.mBaseVal;
    mAnimVal = aLength.mAnimVal;
    mSpecifiedUnitType = aLength.mSpecifiedUnitType;
    mIsAnimated = aLength.mIsAnimated;
    mIsBaseSet = aLength.mIsBaseSet;
    return *this;
  }

  nsresult SetBaseValueString(const nsAString& aValue, SVGElement* aSVGElement,
                              bool aDoSetAttr);
  void GetBaseValueString(nsAString& aValue) const;
  void GetAnimValueString(nsAString& aValue) const;

  float GetBaseValue(SVGElement* aSVGElement) const {
    return mBaseVal * GetPixelsPerUnit(aSVGElement, mSpecifiedUnitType);
  }

  float GetAnimValue(SVGElement* aSVGElement) const {
    return mAnimVal * GetPixelsPerUnit(aSVGElement, mSpecifiedUnitType);
  }
  float GetAnimValue(nsIFrame* aFrame) const {
    return mAnimVal * GetPixelsPerUnit(aFrame, mSpecifiedUnitType);
  }
  float GetAnimValue(SVGViewportElement* aCtx) const {
    return mAnimVal * GetPixelsPerUnit(aCtx, mSpecifiedUnitType);
  }
  float GetAnimValue(const UserSpaceMetrics& aMetrics) const {
    return mAnimVal * GetPixelsPerUnit(aMetrics, mSpecifiedUnitType);
  }

  uint8_t GetCtxType() const { return mCtxType; }
  uint8_t GetSpecifiedUnitType() const { return mSpecifiedUnitType; }
  bool IsPercentage() const {
    return mSpecifiedUnitType ==
           mozilla::dom::SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE;
  }
  float GetAnimValInSpecifiedUnits() const { return mAnimVal; }
  float GetBaseValInSpecifiedUnits() const { return mBaseVal; }

  float GetBaseValue(SVGViewportElement* aCtx) const {
    return mBaseVal * GetPixelsPerUnit(aCtx, mSpecifiedUnitType);
  }

  bool HasBaseVal() const { return mIsBaseSet; }
  // Returns true if the animated value of this length has been explicitly
  // set (either by animation, or by taking on the base value which has been
  // explicitly set by markup or a DOM call), false otherwise.
  // If this returns false, the animated value is still valid, that is,
  // usable, and represents the default base value of the attribute.
  bool IsExplicitlySet() const { return mIsAnimated || mIsBaseSet; }

  already_AddRefed<mozilla::dom::DOMSVGAnimatedLength> ToDOMAnimatedLength(
      SVGElement* aSVGElement);

  mozilla::UniquePtr<SMILAttr> ToSMILAttr(SVGElement* aSVGElement);

 private:
  float mAnimVal;
  float mBaseVal;
  uint8_t mSpecifiedUnitType;
  uint8_t mAttrEnum;  // element specified tracking for attribute
  uint8_t mCtxType;   // X, Y or Unspecified
  bool mIsAnimated : 1;
  bool mIsBaseSet : 1;

  // These APIs returns the number of user-unit pixels per unit of the
  // given type, in a given context (frame/element/etc).
  float GetPixelsPerUnit(nsIFrame* aFrame, uint8_t aUnitType) const;
  float GetPixelsPerUnit(const UserSpaceMetrics& aMetrics,
                         uint8_t aUnitType) const;
  float GetPixelsPerUnit(SVGElement* aSVGElement, uint8_t aUnitType) const;
  float GetPixelsPerUnit(SVGViewportElement* aCtx, uint8_t aUnitType) const;

  // SetBaseValue and SetAnimValue set the value in user units. This may fail
  // if unit conversion fails e.g. conversion to ex or em units where the
  // font-size is 0.
  // SetBaseValueInSpecifiedUnits and SetAnimValueInSpecifiedUnits do not
  // perform unit conversion and are therefore infallible.
  nsresult SetBaseValue(float aValue, SVGElement* aSVGElement, bool aDoSetAttr);
  void SetBaseValueInSpecifiedUnits(float aValue, SVGElement* aSVGElement,
                                    bool aDoSetAttr);
  nsresult SetAnimValue(float aValue, SVGElement* aSVGElement);
  void SetAnimValueInSpecifiedUnits(float aValue, SVGElement* aSVGElement);
  nsresult NewValueSpecifiedUnits(uint16_t aUnitType,
                                  float aValueInSpecifiedUnits,
                                  SVGElement* aSVGElement);
  nsresult ConvertToSpecifiedUnits(uint16_t aUnitType, SVGElement* aSVGElement);
  already_AddRefed<DOMSVGLength> ToDOMBaseVal(SVGElement* aSVGElement);
  already_AddRefed<DOMSVGLength> ToDOMAnimVal(SVGElement* aSVGElement);

 public:
  struct SMILLength : public SMILAttr {
   public:
    SMILLength(SVGAnimatedLength* aVal, SVGElement* aSVGElement)
        : mVal(aVal), mSVGElement(aSVGElement) {}

    // These will stay alive because a SMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    SVGAnimatedLength* mVal;
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

#endif  //  mozilla_SVGAnimatedLength_h
