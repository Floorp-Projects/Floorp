/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGLENGTH2_H__
#define __NS_SVGLENGTH2_H__

#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/SVGLengthBinding.h"
#include "nsCoord.h"
#include "nsCycleCollectionParticipant.h"
#include "nsError.h"
#include "nsISMILAttr.h"
#include "nsMathUtils.h"
#include "nsSVGElement.h"
#include "SVGContentUtils.h"
#include "mozilla/gfx/Rect.h"

class nsIFrame;
class nsSMILValue;

namespace mozilla {
class DOMSVGLength;
namespace dom {
class SVGAnimatedLength;
class SVGAnimationElement;
class SVGViewportElement;
} // namespace dom
} // namespace mozilla

namespace mozilla {
namespace dom {

class UserSpaceMetrics
{
public:
  virtual ~UserSpaceMetrics() {}

  virtual float GetEmLength() const = 0;
  virtual float GetExLength() const = 0;
  virtual float GetAxisLength(uint8_t aCtxType) const = 0;
};

class UserSpaceMetricsWithSize : public UserSpaceMetrics
{
public:
  virtual gfx::Size GetSize() const = 0;
  virtual float GetAxisLength(uint8_t aCtxType) const override;
};

class SVGElementMetrics : public UserSpaceMetrics
{
public:
  typedef mozilla::dom::SVGViewportElement SVGViewportElement;

  explicit SVGElementMetrics(nsSVGElement* aSVGElement,
                             SVGViewportElement* aCtx = nullptr);

  virtual float GetEmLength() const override;
  virtual float GetExLength() const override;
  virtual float GetAxisLength(uint8_t aCtxType) const override;

private:
  bool EnsureCtx() const;

  nsSVGElement* mSVGElement;
  mutable SVGViewportElement* mCtx;
};

class NonSVGFrameUserSpaceMetrics : public UserSpaceMetricsWithSize
{
public:
  explicit NonSVGFrameUserSpaceMetrics(nsIFrame* aFrame);

  virtual float GetEmLength() const override;
  virtual float GetExLength() const override;
  virtual gfx::Size GetSize() const override;

private:
  nsIFrame* mFrame;
};

} // namespace dom
} // namespace mozilla

class nsSVGLength2
{
  friend class mozilla::dom::SVGAnimatedLength;
  friend class mozilla::DOMSVGLength;
  typedef mozilla::dom::UserSpaceMetrics UserSpaceMetrics;
  typedef mozilla::dom::SVGViewportElement SVGViewportElement;

public:
  void Init(uint8_t aCtxType = SVGContentUtils::XY,
            uint8_t aAttrEnum = 0xff,
            float aValue = 0,
            uint8_t aUnitType =
              mozilla::dom::SVGLength_Binding::SVG_LENGTHTYPE_NUMBER) {
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
    { return mBaseVal * GetPixelsPerUnit(aSVGElement, mSpecifiedUnitType); }

  float GetAnimValue(nsSVGElement* aSVGElement) const
    { return mAnimVal * GetPixelsPerUnit(aSVGElement, mSpecifiedUnitType); }
  float GetAnimValue(nsIFrame* aFrame) const
    { return mAnimVal * GetPixelsPerUnit(aFrame, mSpecifiedUnitType); }
  float GetAnimValue(SVGViewportElement* aCtx) const
    { return mAnimVal * GetPixelsPerUnit(aCtx, mSpecifiedUnitType); }
  float GetAnimValue(const UserSpaceMetrics& aMetrics) const
    { return mAnimVal * GetPixelsPerUnit(aMetrics, mSpecifiedUnitType); }

  uint8_t GetCtxType() const { return mCtxType; }
  uint8_t GetSpecifiedUnitType() const { return mSpecifiedUnitType; }
  bool IsPercentage() const
    { return mSpecifiedUnitType ==
        mozilla::dom::SVGLength_Binding::SVG_LENGTHTYPE_PERCENTAGE; }
  float GetAnimValInSpecifiedUnits() const { return mAnimVal; }
  float GetBaseValInSpecifiedUnits() const { return mBaseVal; }

  float GetBaseValue(SVGViewportElement* aCtx) const
    { return mBaseVal * GetPixelsPerUnit(aCtx, mSpecifiedUnitType); }

  bool HasBaseVal() const {
    return mIsBaseSet;
  }
  // Returns true if the animated value of this length has been explicitly
  // set (either by animation, or by taking on the base value which has been
  // explicitly set by markup or a DOM call), false otherwise.
  // If this returns false, the animated value is still valid, that is,
  // usable, and represents the default base value of the attribute.
  bool IsExplicitlySet() const
    { return mIsAnimated || mIsBaseSet; }

  already_AddRefed<mozilla::dom::SVGAnimatedLength>
  ToDOMAnimatedLength(nsSVGElement* aSVGElement);

  mozilla::UniquePtr<nsISMILAttr> ToSMILAttr(nsSVGElement* aSVGElement);

private:
  float mAnimVal;
  float mBaseVal;
  uint8_t mSpecifiedUnitType;
  uint8_t mAttrEnum; // element specified tracking for attribute
  uint8_t mCtxType; // X, Y or Unspecified
  bool mIsAnimated:1;
  bool mIsBaseSet:1;

  // These APIs returns the number of user-unit pixels per unit of the
  // given type, in a given context (frame/element/etc).
  float GetPixelsPerUnit(nsIFrame* aFrame, uint8_t aUnitType) const;
  float GetPixelsPerUnit(const UserSpaceMetrics& aMetrics, uint8_t aUnitType) const;
  float GetPixelsPerUnit(nsSVGElement* aSVGElement, uint8_t aUnitType) const;
  float GetPixelsPerUnit(SVGViewportElement* aCtx, uint8_t aUnitType) const;

  // SetBaseValue and SetAnimValue set the value in user units. This may fail
  // if unit conversion fails e.g. conversion to ex or em units where the
  // font-size is 0.
  // SetBaseValueInSpecifiedUnits and SetAnimValueInSpecifiedUnits do not
  // perform unit conversion and are therefore infallible.
  nsresult SetBaseValue(float aValue, nsSVGElement *aSVGElement,
                        bool aDoSetAttr);
  void SetBaseValueInSpecifiedUnits(float aValue, nsSVGElement *aSVGElement,
                                    bool aDoSetAttr);
  nsresult SetAnimValue(float aValue, nsSVGElement *aSVGElement);
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
                                     bool& aPreventCachingOfSandwich) const override;
    virtual nsSMILValue GetBaseValue() const override;
    virtual void ClearAnimValue() override;
    virtual nsresult SetAnimValue(const nsSMILValue& aValue) override;
  };
};

#endif //  __NS_SVGLENGTH2_H__
