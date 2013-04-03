/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGANIMATEDTRANSFORMLIST_H__
#define MOZILLA_SVGANIMATEDTRANSFORMLIST_H__

#include "nsAutoPtr.h"
#include "nsISMILAttr.h"
#include "SVGTransformList.h"

class nsIAtom;
class nsSMILValue;
class nsSVGElement;

namespace mozilla {

namespace dom {
class SVGAnimationElement;
class SVGTransform;
}

/**
 * Class SVGAnimatedTransformList
 *
 * This class is very different to the SVG DOM interface of the same name found
 * in the SVG specification. This is a lightweight internal class - see
 * DOMSVGAnimatedTransformList for the heavier DOM class that wraps instances of
 * this class and implements the SVG specification's SVGAnimatedTransformList
 * DOM interface.
 *
 * Except where noted otherwise, this class' methods take care of keeping the
 * appropriate DOM wrappers in sync (see the comment in
 * DOMSVGAnimatedTransformList::InternalBaseValListWillChangeTo) so that their
 * consumers don't need to concern themselves with that.
 */
class SVGAnimatedTransformList
{
  // friends so that they can get write access to mBaseVal
  friend class dom::SVGTransform;
  friend class DOMSVGTransformList;

public:
  SVGAnimatedTransformList() : mIsAttrSet(false) { }

  /**
   * Because it's so important that mBaseVal and its DOMSVGTransformList wrapper
   * (if any) be kept in sync (see the comment in
   * DOMSVGAnimatedTransformList::InternalBaseValListWillChangeTo), this method
   * returns a const reference. Only our friend classes may get mutable
   * references to mBaseVal.
   */
  const SVGTransformList& GetBaseValue() const {
    return mBaseVal;
  }

  nsresult SetBaseValueString(const nsAString& aValue);

  void ClearBaseValue();

  const SVGTransformList& GetAnimValue() const {
    return mAnimVal ? *mAnimVal : mBaseVal;
  }

  nsresult SetAnimValue(const SVGTransformList& aNewAnimValue,
                        nsSVGElement *aElement);

  void ClearAnimValue(nsSVGElement *aElement);

  bool IsExplicitlySet() const;

  bool IsAnimating() const {
    return !!mAnimVal;
  }

  /// Callers own the returned nsISMILAttr
  nsISMILAttr* ToSMILAttr(nsSVGElement* aSVGElement);

private:

  // mAnimVal is a pointer to allow us to determine if we're being animated or
  // not. Making it a non-pointer member and using mAnimVal.IsEmpty() to check
  // if we're animating is not an option, since that would break animation *to*
  // the empty string (<set to="">).

  SVGTransformList mBaseVal;
  nsAutoPtr<SVGTransformList> mAnimVal;
  bool mIsAttrSet;

  struct SMILAnimatedTransformList : public nsISMILAttr
  {
  public:
    SMILAnimatedTransformList(SVGAnimatedTransformList* aVal,
                              nsSVGElement* aSVGElement)
      : mVal(aVal)
      , mElement(aSVGElement)
    {}

    // nsISMILAttr methods
    virtual nsresult ValueFromString(const nsAString& aStr,
                                     const dom::SVGAnimationElement* aSrcElement,
                                     nsSMILValue& aValue,
                                     bool& aPreventCachingOfSandwich) const;
    virtual nsSMILValue GetBaseValue() const;
    virtual void ClearAnimValue();
    virtual nsresult SetAnimValue(const nsSMILValue& aValue);

  protected:
    static void ParseValue(const nsAString& aSpec,
                           const nsIAtom* aTransformType,
                           nsSMILValue& aResult);
    static int32_t ParseParameterList(const nsAString& aSpec, float* aVars,
                                      int32_t aNVars);

    // These will stay alive because a nsISMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    SVGAnimatedTransformList* mVal;
    nsSVGElement* mElement;
  };
};

} // namespace mozilla

#endif // MOZILLA_SVGANIMATEDTRANSFORMLIST_H__
