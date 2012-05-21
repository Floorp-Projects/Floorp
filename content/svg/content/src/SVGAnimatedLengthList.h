/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGANIMATEDLENGTHLIST_H__
#define MOZILLA_SVGANIMATEDLENGTHLIST_H__

#include "nsAutoPtr.h"
#include "nsISMILAttr.h"
#include "SVGLengthList.h"

class nsISMILAnimationElement;
class nsSMILValue;
class nsSVGElement;

namespace mozilla {

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

  void ClearBaseValue(PRUint32 aAttrEnum);

  const SVGLengthList& GetAnimValue() const {
    return mAnimVal ? *mAnimVal : mBaseVal;
  }

  nsresult SetAnimValue(const SVGLengthList& aValue,
                        nsSVGElement *aElement,
                        PRUint32 aAttrEnum);

  void ClearAnimValue(nsSVGElement *aElement,
                      PRUint32 aAttrEnum);

  bool IsAnimating() const {
    return !!mAnimVal;
  }

  /// Callers own the returned nsISMILAttr
  nsISMILAttr* ToSMILAttr(nsSVGElement* aSVGElement, PRUint8 aAttrEnum,
                          PRUint8 aAxis, bool aCanZeroPadList);

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
                           PRUint8 aAttrEnum,
                           PRUint8 aAxis,
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
    PRUint8 mAttrEnum;
    PRUint8 mAxis;
    bool mCanZeroPadList; // See SVGLengthListAndInfo::CanZeroPadList

    // nsISMILAttr methods
    virtual nsresult ValueFromString(const nsAString& aStr,
                                     const nsISMILAnimationElement* aSrcElement,
                                     nsSMILValue& aValue,
                                     bool& aPreventCachingOfSandwich) const;
    virtual nsSMILValue GetBaseValue() const;
    virtual void ClearAnimValue();
    virtual nsresult SetAnimValue(const nsSMILValue& aValue);
  };
};

} // namespace mozilla

#endif // MOZILLA_SVGANIMATEDLENGTHLIST_H__
