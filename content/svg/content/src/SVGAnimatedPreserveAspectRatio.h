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
 *   Jonathan Watt <jonathan.watt@strath.ac.uk> (original author)
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

#ifndef MOZILLA_SVGANIMATEDPRESERVEASPECTRATIO_H__
#define MOZILLA_SVGANIMATEDPRESERVEASPECTRATIO_H__

#include "nsIDOMSVGPresAspectRatio.h"
#include "nsIDOMSVGAnimPresAspRatio.h"
#include "nsSVGElement.h"
#include "nsDOMError.h"

namespace mozilla {

class SVGAnimatedPreserveAspectRatio;

class SVGPreserveAspectRatio
{
  friend class SVGAnimatedPreserveAspectRatio;

public:
  SVGPreserveAspectRatio()
    : mAlign(0)
    , mMeetOrSlice(0)
    , mDefer(false)
  {};

  nsresult SetAlign(PRUint16 aAlign) {
    if (aAlign < nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_NONE ||
        aAlign > nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMAXYMAX)
      return NS_ERROR_FAILURE;
    mAlign = static_cast<PRUint8>(aAlign);
    return NS_OK;
  };

  PRUint16 GetAlign() const {
    return mAlign;
  };

  nsresult SetMeetOrSlice(PRUint16 aMeetOrSlice) {
    if (aMeetOrSlice < nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET ||
        aMeetOrSlice > nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_SLICE)
      return NS_ERROR_FAILURE;
    mMeetOrSlice = static_cast<PRUint8>(aMeetOrSlice);
    return NS_OK;
  };

  PRUint16 GetMeetOrSlice() const {
    return mMeetOrSlice;
  };

  void SetDefer(bool aDefer) {
    mDefer = aDefer;
  };

  bool GetDefer() const {
    return mDefer;
  };

private:
  PRUint8 mAlign;
  PRUint8 mMeetOrSlice;
  bool mDefer;
};

class SVGAnimatedPreserveAspectRatio
{
public:
  void Init() {
    mBaseVal.mAlign = nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID;
    mBaseVal.mMeetOrSlice = nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET;
    mBaseVal.mDefer = false;
    mAnimVal = mBaseVal;
    mIsAnimated = false;
    mIsBaseSet = false;
  }

  nsresult SetBaseValueString(const nsAString& aValue,
                              nsSVGElement *aSVGElement);
  void GetBaseValueString(nsAString& aValue);

  nsresult SetBaseAlign(PRUint16 aAlign, nsSVGElement *aSVGElement);
  nsresult SetBaseMeetOrSlice(PRUint16 aMeetOrSlice, nsSVGElement *aSVGElement);
  void SetAnimValue(PRUint64 aPackedValue, nsSVGElement *aSVGElement);

  const SVGPreserveAspectRatio &GetBaseValue() const
    { return mBaseVal; }
  const SVGPreserveAspectRatio &GetAnimValue() const
    { return mAnimVal; }
  bool IsAnimated() const
    { return mIsAnimated; }
  bool IsExplicitlySet() const
    { return mIsAnimated || mIsBaseSet; }

  nsresult ToDOMAnimatedPreserveAspectRatio(
    nsIDOMSVGAnimatedPreserveAspectRatio **aResult,
    nsSVGElement* aSVGElement);
  // Returns a new nsISMILAttr object that the caller must delete
  nsISMILAttr* ToSMILAttr(nsSVGElement* aSVGElement);

private:

  SVGPreserveAspectRatio mAnimVal;
  SVGPreserveAspectRatio mBaseVal;
  bool mIsAnimated;
  bool mIsBaseSet;

  nsresult ToDOMBaseVal(nsIDOMSVGPreserveAspectRatio **aResult,
                        nsSVGElement* aSVGElement);
  nsresult ToDOMAnimVal(nsIDOMSVGPreserveAspectRatio **aResult,
                        nsSVGElement* aSVGElement);

public:
  struct DOMBaseVal : public nsIDOMSVGPreserveAspectRatio
  {
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(DOMBaseVal)

    DOMBaseVal(SVGAnimatedPreserveAspectRatio* aVal, nsSVGElement *aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}
    
    SVGAnimatedPreserveAspectRatio* mVal; // kept alive because it belongs to mSVGElement
    nsRefPtr<nsSVGElement> mSVGElement;
    
    NS_IMETHOD GetAlign(PRUint16* aAlign)
      { *aAlign = mVal->GetBaseValue().GetAlign(); return NS_OK; }
    NS_IMETHOD SetAlign(PRUint16 aAlign)
      { return mVal->SetBaseAlign(aAlign, mSVGElement); }

    NS_IMETHOD GetMeetOrSlice(PRUint16* aMeetOrSlice)
      { *aMeetOrSlice = mVal->GetBaseValue().GetMeetOrSlice(); return NS_OK; }
    NS_IMETHOD SetMeetOrSlice(PRUint16 aMeetOrSlice)
      { return mVal->SetBaseMeetOrSlice(aMeetOrSlice, mSVGElement); }
  };

  struct DOMAnimVal : public nsIDOMSVGPreserveAspectRatio
  {
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(DOMAnimVal)

    DOMAnimVal(SVGAnimatedPreserveAspectRatio* aVal, nsSVGElement *aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}
    
    SVGAnimatedPreserveAspectRatio* mVal; // kept alive because it belongs to mSVGElement
    nsRefPtr<nsSVGElement> mSVGElement;
    
    // Script may have modified animation parameters or timeline -- DOM getters
    // need to flush any resample requests to reflect these modifications.
    NS_IMETHOD GetAlign(PRUint16* aAlign)
    {
      mSVGElement->FlushAnimations();
      *aAlign = mVal->GetAnimValue().GetAlign();
      return NS_OK;
    }
    NS_IMETHOD SetAlign(PRUint16 aAlign)
      { return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR; }

    NS_IMETHOD GetMeetOrSlice(PRUint16* aMeetOrSlice)
    {
      mSVGElement->FlushAnimations();
      *aMeetOrSlice = mVal->GetAnimValue().GetMeetOrSlice();
      return NS_OK;
    }
    NS_IMETHOD SetMeetOrSlice(PRUint16 aValue)
      { return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR; }
  };

  struct DOMAnimPAspectRatio : public nsIDOMSVGAnimatedPreserveAspectRatio
  {
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(DOMAnimPAspectRatio)

    DOMAnimPAspectRatio(SVGAnimatedPreserveAspectRatio* aVal,
                        nsSVGElement *aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}

    // kept alive because it belongs to content:
    SVGAnimatedPreserveAspectRatio* mVal;

    nsRefPtr<nsSVGElement> mSVGElement;

    NS_IMETHOD GetBaseVal(nsIDOMSVGPreserveAspectRatio **aBaseVal)
      { return mVal->ToDOMBaseVal(aBaseVal, mSVGElement); }

    NS_IMETHOD GetAnimVal(nsIDOMSVGPreserveAspectRatio **aAnimVal)
      { return mVal->ToDOMAnimVal(aAnimVal, mSVGElement); }
  };

  struct SMILPreserveAspectRatio : public nsISMILAttr
  {
  public:
    SMILPreserveAspectRatio(SVGAnimatedPreserveAspectRatio* aVal,
                            nsSVGElement* aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}

    // These will stay alive because a nsISMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    SVGAnimatedPreserveAspectRatio* mVal;
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

} // namespace mozilla

#endif // MOZILLA_SVGANIMATEDPRESERVEASPECTRATIO_H__
