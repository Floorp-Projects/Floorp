/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SVG Project code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef MOZILLA_SVGANIMATEDPOINTLIST_H__
#define MOZILLA_SVGANIMATEDPOINTLIST_H__

#include "SVGPointList.h"

class nsSVGElement;

#ifdef MOZ_SMIL
#include "nsISMILAttr.h"
#endif // MOZ_SMIL

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

#ifdef MOZ_SMIL
  /// Callers own the returned nsISMILAttr
  nsISMILAttr* ToSMILAttr(nsSVGElement* aElement);
#endif // MOZ_SMIL

private:

  // mAnimVal is a pointer to allow us to determine if we're being animated or
  // not. Making it a non-pointer member and using mAnimVal.IsEmpty() to check
  // if we're animating is not an option, since that would break animation *to*
  // the empty string (<set to="">).

  SVGPointList mBaseVal;
  nsAutoPtr<SVGPointList> mAnimVal;

#ifdef MOZ_SMIL
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
#endif // MOZ_SMIL
};

} // namespace mozilla

#endif // MOZILLA_SVGANIMATEDPOINTLIST_H__
