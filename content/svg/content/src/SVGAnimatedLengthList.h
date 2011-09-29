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

#ifndef MOZILLA_SVGANIMATEDLENGTHLIST_H__
#define MOZILLA_SVGANIMATEDLENGTHLIST_H__

#include "SVGLengthList.h"

class nsSVGElement;

#ifdef MOZ_SMIL
#include "nsISMILAttr.h"
#endif // MOZ_SMIL

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

#ifdef MOZ_SMIL
  /// Callers own the returned nsISMILAttr
  nsISMILAttr* ToSMILAttr(nsSVGElement* aSVGElement, PRUint8 aAttrEnum,
                          PRUint8 aAxis, bool aCanZeroPadList);
#endif // MOZ_SMIL

private:

  // mAnimVal is a pointer to allow us to determine if we're being animated or
  // not. Making it a non-pointer member and using mAnimVal.IsEmpty() to check
  // if we're animating is not an option, since that would break animation *to*
  // the empty string (<set to="">).

  SVGLengthList mBaseVal;
  nsAutoPtr<SVGLengthList> mAnimVal;

#ifdef MOZ_SMIL
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
#endif // MOZ_SMIL
};

} // namespace mozilla

#endif // MOZILLA_SVGANIMATEDLENGTHLIST_H__
