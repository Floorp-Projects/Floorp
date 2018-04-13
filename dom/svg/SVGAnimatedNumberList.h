/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGANIMATEDNUMBERLIST_H__
#define MOZILLA_SVGANIMATEDNUMBERLIST_H__

#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"
#include "nsAutoPtr.h"
#include "nsISMILAttr.h"
#include "SVGNumberList.h"

class nsSMILValue;
class nsSVGElement;

namespace mozilla {

namespace dom {
class SVGAnimationElement;
} // namespace dom

/**
 * Class SVGAnimatedNumberList
 *
 * This class is very different to the SVG DOM interface of the same name found
 * in the SVG specification. This is a lightweight internal class - see
 * DOMSVGAnimatedNumberList for the heavier DOM class that wraps instances of
 * this class and implements the SVG specification's SVGAnimatedNumberList DOM
 * interface.
 *
 * Except where noted otherwise, this class' methods take care of keeping the
 * appropriate DOM wrappers in sync (see the comment in
 * DOMSVGAnimatedNumberList::InternalBaseValListWillChangeTo) so that their
 * consumers don't need to concern themselves with that.
 */
class SVGAnimatedNumberList
{
  // friends so that they can get write access to mBaseVal
  friend class DOMSVGNumber;
  friend class DOMSVGNumberList;

public:
  SVGAnimatedNumberList() {}

  /**
   * Because it's so important that mBaseVal and its DOMSVGNumberList wrapper
   * (if any) be kept in sync (see the comment in
   * DOMSVGAnimatedNumberList::InternalBaseValListWillChangeTo), this method
   * returns a const reference. Only our friend classes may get mutable
   * references to mBaseVal.
   */
  const SVGNumberList& GetBaseValue() const {
    return mBaseVal;
  }

  nsresult SetBaseValueString(const nsAString& aValue);

  void ClearBaseValue(uint32_t aAttrEnum);

  const SVGNumberList& GetAnimValue() const {
    return mAnimVal ? *mAnimVal : mBaseVal;
  }

  nsresult SetAnimValue(const SVGNumberList& aValue,
                        nsSVGElement *aElement,
                        uint32_t aAttrEnum);

  void ClearAnimValue(nsSVGElement *aElement,
                      uint32_t aAttrEnum);

  // Returns true if the animated value of this list has been explicitly
  // set (either by animation, or by taking on the base value which has been
  // explicitly set by markup or a DOM call), false otherwise.
  // If this returns false, the animated value is still valid, that is,
  // usable, and represents the default base value of the attribute.
  bool IsExplicitlySet() const
    { return !!mAnimVal || mIsBaseSet; }

  bool IsAnimating() const {
    return !!mAnimVal;
  }

  UniquePtr<nsISMILAttr> ToSMILAttr(nsSVGElement* aSVGElement,
                                    uint8_t aAttrEnum);

private:

  // mAnimVal is a pointer to allow us to determine if we're being animated or
  // not. Making it a non-pointer member and using mAnimVal.IsEmpty() to check
  // if we're animating is not an option, since that would break animation *to*
  // the empty string (<set to="">).

  SVGNumberList mBaseVal;
  nsAutoPtr<SVGNumberList> mAnimVal;
  bool mIsBaseSet;

  struct SMILAnimatedNumberList : public nsISMILAttr
  {
  public:
    SMILAnimatedNumberList(SVGAnimatedNumberList* aVal,
                           nsSVGElement* aSVGElement,
                           uint8_t aAttrEnum)
      : mVal(aVal)
      , mElement(aSVGElement)
      , mAttrEnum(aAttrEnum)
    {}

    // These will stay alive because a nsISMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    SVGAnimatedNumberList* mVal;
    nsSVGElement* mElement;
    uint8_t mAttrEnum;

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

#endif // MOZILLA_SVGANIMATEDNUMBERLIST_H__
