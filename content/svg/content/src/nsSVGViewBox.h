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
 * The Initial Developer of the Original Code is
 * Jonathan Watt.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Craig Topper <craig.topper@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef __NS_SVGVIEWBOX_H__
#define __NS_SVGVIEWBOX_H__

#include "nsIDOMSVGRect.h"
#include "nsIDOMSVGAnimatedRect.h"
#include "nsSVGElement.h"
#include "nsDOMError.h"

struct nsSVGViewBoxRect
{
  float x, y;
  float width, height;

  nsSVGViewBoxRect() : x(0), y(0), width(0), height(0) {}
  nsSVGViewBoxRect(float aX, float aY, float aWidth, float aHeight) :
    x(aX), y(aY), width(aWidth), height(aHeight) {}
  nsSVGViewBoxRect(const nsSVGViewBoxRect& rhs) :
    x(rhs.x), y(rhs.y), width(rhs.width), height(rhs.height) {}
  bool operator==(const nsSVGViewBoxRect& aOther) const;
};

class nsSVGViewBox
{

public:

  void Init();

  // Used by element to tell if viewBox is defined
  bool IsValid() const
    { return (mHasBaseVal || mAnimVal); }

  const nsSVGViewBoxRect& GetBaseValue() const
    { return mBaseVal; }
  void SetBaseValue(float aX, float aY, float aWidth, float aHeight,
                    nsSVGElement *aSVGElement);

  const nsSVGViewBoxRect& GetAnimValue() const
    { return mAnimVal ? *mAnimVal : mBaseVal; }
  void SetAnimValue(float aX, float aY, float aWidth, float aHeight,
                    nsSVGElement *aSVGElement);

  nsresult SetBaseValueString(const nsAString& aValue,
                              nsSVGElement *aSVGElement);
  void GetBaseValueString(nsAString& aValue) const;

  nsresult ToDOMAnimatedRect(nsIDOMSVGAnimatedRect **aResult,
                             nsSVGElement *aSVGElement);
  // Returns a new nsISMILAttr object that the caller must delete
  nsISMILAttr* ToSMILAttr(nsSVGElement* aSVGElement);
  
private:

  nsSVGViewBoxRect mBaseVal;
  nsAutoPtr<nsSVGViewBoxRect> mAnimVal;
  bool mHasBaseVal;

  struct DOMBaseVal : public nsIDOMSVGRect
  {
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(DOMBaseVal)

    DOMBaseVal(nsSVGViewBox *aVal, nsSVGElement *aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}

    nsSVGViewBox* mVal; // kept alive because it belongs to content
    nsRefPtr<nsSVGElement> mSVGElement;

    NS_IMETHOD GetX(float *aX)
      { *aX = mVal->GetBaseValue().x; return NS_OK; }
    NS_IMETHOD GetY(float *aY)
      { *aY = mVal->GetBaseValue().y; return NS_OK; }
    NS_IMETHOD GetWidth(float *aWidth)
      { *aWidth = mVal->GetBaseValue().width; return NS_OK; }
    NS_IMETHOD GetHeight(float *aHeight)
      { *aHeight = mVal->GetBaseValue().height; return NS_OK; }

    NS_IMETHOD SetX(float aX);
    NS_IMETHOD SetY(float aY);
    NS_IMETHOD SetWidth(float aWidth);
    NS_IMETHOD SetHeight(float aHeight);
  };

  struct DOMAnimVal : public nsIDOMSVGRect
  {
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(DOMAnimVal)

    DOMAnimVal(nsSVGViewBox *aVal, nsSVGElement *aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}

    nsSVGViewBox* mVal; // kept alive because it belongs to content
    nsRefPtr<nsSVGElement> mSVGElement;

    // Script may have modified animation parameters or timeline -- DOM getters
    // need to flush any resample requests to reflect these modifications.
    NS_IMETHOD GetX(float *aX)
    {
      mSVGElement->FlushAnimations();
      *aX = mVal->GetAnimValue().x;
      return NS_OK;
    }
    NS_IMETHOD GetY(float *aY)
    {
      mSVGElement->FlushAnimations();
      *aY = mVal->GetAnimValue().y;
      return NS_OK;
    }
    NS_IMETHOD GetWidth(float *aWidth)
    {
      mSVGElement->FlushAnimations();
      *aWidth = mVal->GetAnimValue().width;
      return NS_OK;
    }
    NS_IMETHOD GetHeight(float *aHeight)
    {
      mSVGElement->FlushAnimations();
      *aHeight = mVal->GetAnimValue().height;
      return NS_OK;
    }

    NS_IMETHOD SetX(float aX)
      { return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR; }
    NS_IMETHOD SetY(float aY)
      { return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR; }
    NS_IMETHOD SetWidth(float aWidth)
      { return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR; }
    NS_IMETHOD SetHeight(float aHeight)
      { return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR; }
  };

public:
  struct DOMAnimatedRect : public nsIDOMSVGAnimatedRect
  {
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(DOMAnimatedRect)

    DOMAnimatedRect(nsSVGViewBox *aVal, nsSVGElement *aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}

    nsSVGViewBox* mVal; // kept alive because it belongs to content
    nsRefPtr<nsSVGElement> mSVGElement;

    NS_IMETHOD GetBaseVal(nsIDOMSVGRect **aResult);
    NS_IMETHOD GetAnimVal(nsIDOMSVGRect **aResult);
  };

  struct SMILViewBox : public nsISMILAttr
  {
  public:
    SMILViewBox(nsSVGViewBox* aVal, nsSVGElement* aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}

    // These will stay alive because a nsISMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    nsSVGViewBox* mVal;
    nsSVGElement* mSVGElement;

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

#endif // __NS_SVGVIEWBOX_H__
