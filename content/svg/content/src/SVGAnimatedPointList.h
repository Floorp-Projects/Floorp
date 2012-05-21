/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGANIMATEDPOINTLIST_H__
#define MOZILLA_SVGANIMATEDPOINTLIST_H__

#include "nsAutoPtr.h"
#include "nsISMILAttr.h"
#include "SVGPointList.h"

class nsISMILAnimationElement;
class nsSMILValue;
class nsSVGElement;

namespace mozilla {

/**
 * Class SVGAnimatedPointList
 *
 * Despite the fact that no SVGAnimatedPointList interface or objects exist
 * in the SVG specification (unlike e.g. SVGAnimated*Length*List), we
 * nevertheless have this internal class. (Note that there is an
 * SVGAnimatedPoints interface, but that's quite different to
 * SVGAnimatedLengthList since it is inherited by elements, as opposed to
 * elements having members of that type.) The reason that we have this class is
 * to provide a single locked down point of entry to the SVGPointList objects,
 * which helps ensure that the DOM wrappers for SVGPointList objects' are
 * always kept in sync. This is vitally important (see the comment in
 * DOMSVGPointList::InternalListWillChangeTo) and frees consumers from having
 * to know or worry about wrappers (or forget about them!) for the most part.
 */
class SVGAnimatedPointList
{
  // friends so that they can get write access to mBaseVal and mAnimVal
  friend class DOMSVGPoint;
  friend class DOMSVGPointList;

public:
  SVGAnimatedPointList() {}

  /**
   * Because it's so important that mBaseVal and its DOMSVGPointList wrapper
   * (if any) be kept in sync (see the comment in
   * DOMSVGPointList::InternalListWillChangeTo), this method returns a const
   * reference. Only our friend classes may get mutable references to mBaseVal.
   */
  const SVGPointList& GetBaseValue() const {
    return mBaseVal;
  }

  nsresult SetBaseValueString(const nsAString& aValue);

  void ClearBaseValue();

  /**
   * const! See comment for GetBaseValue!
   */
  const SVGPointList& GetAnimValue() const {
    return mAnimVal ? *mAnimVal : mBaseVal;
  }

  nsresult SetAnimValue(const SVGPointList& aValue,
                        nsSVGElement *aElement);

  void ClearAnimValue(nsSVGElement *aElement);

  /**
   * Needed for correct DOM wrapper construction since GetAnimValue may
   * actually return the baseVal!
   */
  void *GetBaseValKey() const {
    return (void*)&mBaseVal;
  }
  void *GetAnimValKey() const {
    return (void*)&mAnimVal;
  }

  bool IsAnimating() const {
    return !!mAnimVal;
  }

  /// Callers own the returned nsISMILAttr
  nsISMILAttr* ToSMILAttr(nsSVGElement* aElement);

private:

  // mAnimVal is a pointer to allow us to determine if we're being animated or
  // not. Making it a non-pointer member and using mAnimVal.IsEmpty() to check
  // if we're animating is not an option, since that would break animation *to*
  // the empty string (<set to="">).

  SVGPointList mBaseVal;
  nsAutoPtr<SVGPointList> mAnimVal;

  struct SMILAnimatedPointList : public nsISMILAttr
  {
  public:
    SMILAnimatedPointList(SVGAnimatedPointList* aVal,
                          nsSVGElement* aElement)
      : mVal(aVal)
      , mElement(aElement)
    {}

    // These will stay alive because a nsISMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    SVGAnimatedPointList *mVal;
    nsSVGElement *mElement;

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

#endif // MOZILLA_SVGANIMATEDPOINTLIST_H__
