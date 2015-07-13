/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGANIMATEDLENGTHLIST_H__
#define MOZILLA_SVGANIMATEDLENGTHLIST_H__

#include "mozilla/Attributes.h"
#include "nsAutoPtr.h"
#include "nsISMILAttr.h"
#include "SVGLengthList.h"

class nsSMILValue;
class nsSVGElement;

namespace mozilla {

namespace dom {
class SVGAnimationElement;
} // namespace dom

/**
 * Class SVGAnimatedLengthList
 *
 * This class is very different to the SVG DOM interface of the same name found
 * in the SVG specification. This is a lightweight internal class - see
 * DOMSVGAnimatedLengthList for the heavier DOM class that wraps instances of
 * this class and implements the SVG specification's SVGAnimatedLengthList DOM
 * interface.
 *
 * Except where noted otherwise, this class' methods take care of keeping the
 * appropriate DOM wrappers in sync (see the comment in
 * DOMSVGAnimatedLengthList::InternalBaseValListWillChangeTo) so that their
 * consumers don't need to concern themselves with that.
 */
class SVGAnimatedLengthList
{
  // friends so that they can get write access to mBaseVal
  friend class DOMSVGLength;
  friend class DOMSVGLengthList;

public:
  SVGAnimatedLengthList() {}

  /**
   * Because it's so important that mBaseVal and its DOMSVGLengthList wrapper
   * (if any) be kept in sync (see the comment in
   * DOMSVGAnimatedLengthList::InternalBaseValListWillChangeTo), this method
   * returns a const reference. Only our friend classes may get mutable
   * references to mBaseVal.
   */
  const SVGLengthList& GetBaseValue() const {
    return mBaseVal;
  }

  nsresult SetBaseValueString(const nsAString& aValue);

  void ClearBaseValue(uint32_t aAttrEnum);

  const SVGLengthList& GetAnimValue() const {
    return mAnimVal ? *mAnimVal : mBaseVal;
  }

  nsresult SetAnimValue(const SVGLengthList& aValue,
                        nsSVGElement *aElement,
                        uint32_t aAttrEnum);

  void ClearAnimValue(nsSVGElement *aElement,
                      uint32_t aAttrEnum);

  bool IsAnimating() const {
    return !!mAnimVal;
  }

  /// Callers own the returned nsISMILAttr
  nsISMILAttr* ToSMILAttr(nsSVGElement* aSVGElement, uint8_t aAttrEnum,
                          uint8_t aAxis, bool aCanZeroPadList);

private:

  // mAnimVal is a pointer to allow us to determine if we're being animated or
  // not. Making it a non-pointer member and using mAnimVal.IsEmpty() to check
  // if we're animating is not an option, since that would break animation *to*
  // the empty string (<set to="">).

  SVGLengthList mBaseVal;
  nsAutoPtr<SVGLengthList> mAnimVal;

  struct SMILAnimatedLengthList : public nsISMILAttr
  {
  public:
    SMILAnimatedLengthList(SVGAnimatedLengthList* aVal,
                           nsSVGElement* aSVGElement,
                           uint8_t aAttrEnum,
                           uint8_t aAxis,
                           bool aCanZeroPadList)
      : mVal(aVal)
      , mElement(aSVGElement)
      , mAttrEnum(aAttrEnum)
      , mAxis(aAxis)
      , mCanZeroPadList(aCanZeroPadList)
    {}

    // These will stay alive because a nsISMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    SVGAnimatedLengthList* mVal;
    nsSVGElement* mElement;
    uint8_t mAttrEnum;
    uint8_t mAxis;
    bool mCanZeroPadList; // See SVGLengthListAndInfo::CanZeroPadList

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

} // namespace mozilla

#endif // MOZILLA_SVGANIMATEDLENGTHLIST_H__
