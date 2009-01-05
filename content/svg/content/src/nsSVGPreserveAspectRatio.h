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

#ifndef __NS_SVGPRESERVEASPECTRATIO_H__
#define __NS_SVGPRESERVEASPECTRATIO_H__

#include "nsIDOMSVGPresAspectRatio.h"
#include "nsIDOMSVGAnimPresAspRatio.h"
#include "nsSVGElement.h"
#include "nsDOMError.h"

class nsSVGPreserveAspectRatio
{
public:
  class PreserveAspectRatio
  {
  friend class nsSVGPreserveAspectRatio;

  public:
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

    void SetDefer(PRBool aDefer) {
      mDefer = aDefer;
    };

    PRBool GetDefer() const {
      return mDefer;
    };

  private:
    PRUint8 mAlign;
    PRUint8 mMeetOrSlice;
    PRPackedBool mDefer;
  };

  void Init() {
    mBaseVal.mAlign = nsIDOMSVGPreserveAspectRatio::SVG_PRESERVEASPECTRATIO_XMIDYMID;
    mBaseVal.mMeetOrSlice = nsIDOMSVGPreserveAspectRatio::SVG_MEETORSLICE_MEET;
    mBaseVal.mDefer = PR_FALSE;
    mAnimVal = mBaseVal;
  }

  nsresult SetBaseValueString(const nsAString& aValue,
                              nsSVGElement *aSVGElement,
                              PRBool aDoSetAttr);
  void GetBaseValueString(nsAString& aValue);

  nsresult SetBaseAlign(PRUint16 aAlign, nsSVGElement *aSVGElement);
  nsresult SetBaseMeetOrSlice(PRUint16 aMeetOrSlice, nsSVGElement *aSVGElement);
  const PreserveAspectRatio &GetBaseValue() const
    { return mBaseVal; }
  const PreserveAspectRatio &GetAnimValue() const
    { return mAnimVal; }

  nsresult ToDOMAnimatedPreserveAspectRatio(
    nsIDOMSVGAnimatedPreserveAspectRatio **aResult,
    nsSVGElement* aSVGElement);

private:

  PreserveAspectRatio mAnimVal;
  PreserveAspectRatio mBaseVal;

  nsresult ToDOMBaseVal(nsIDOMSVGPreserveAspectRatio **aResult,
                        nsSVGElement* aSVGElement);
  nsresult ToDOMAnimVal(nsIDOMSVGPreserveAspectRatio **aResult,
                        nsSVGElement* aSVGElement);

  struct DOMBaseVal;
  friend struct DOMBaseVal;
  struct DOMBaseVal : public nsIDOMSVGPreserveAspectRatio
  {
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(DOMBaseVal)

    DOMBaseVal(nsSVGPreserveAspectRatio* aVal, nsSVGElement *aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}
    
    nsSVGPreserveAspectRatio* mVal; // kept alive because it belongs to mSVGElement
    nsRefPtr<nsSVGElement> mSVGElement;
    
    NS_IMETHOD GetAlign(PRUint16* aAlign)
      { *aAlign = mVal->GetBaseValue().mAlign; return NS_OK; }
    NS_IMETHOD SetAlign(PRUint16 aAlign)
      { return mVal->SetBaseAlign(aAlign, mSVGElement); }

    NS_IMETHOD GetMeetOrSlice(PRUint16* aMeetOrSlice)
      { *aMeetOrSlice = mVal->GetBaseValue().mMeetOrSlice; return NS_OK; }
    NS_IMETHOD SetMeetOrSlice(PRUint16 aMeetOrSlice)
      { return mVal->SetBaseMeetOrSlice(aMeetOrSlice, mSVGElement); }
  };

  struct DOMAnimVal;
  friend struct DOMAnimVal;
  struct DOMAnimVal : public nsIDOMSVGPreserveAspectRatio
  {
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(DOMAnimVal)

    DOMAnimVal(nsSVGPreserveAspectRatio* aVal, nsSVGElement *aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}
    
    nsSVGPreserveAspectRatio* mVal; // kept alive because it belongs to mSVGElement
    nsRefPtr<nsSVGElement> mSVGElement;
    
    NS_IMETHOD GetAlign(PRUint16* aAlign)
      { *aAlign = mVal->GetBaseValue().mAlign; return NS_OK; }
    NS_IMETHOD SetAlign(PRUint16 aAlign)
      { return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR; }

    NS_IMETHOD GetMeetOrSlice(PRUint16* aMeetOrSlice)
      { *aMeetOrSlice = mVal->GetBaseValue().mMeetOrSlice; return NS_OK; }
    NS_IMETHOD SetMeetOrSlice(PRUint16 aValue)
      { return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR; }
  };

  struct DOMAnimPAspectRatio : public nsIDOMSVGAnimatedPreserveAspectRatio
  {
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(DOMAnimPAspectRatio)

    DOMAnimPAspectRatio(nsSVGPreserveAspectRatio* aVal, nsSVGElement *aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}
    
    nsSVGPreserveAspectRatio* mVal; // kept alive because it belongs to content
    nsRefPtr<nsSVGElement> mSVGElement;

    NS_IMETHOD GetBaseVal(nsIDOMSVGPreserveAspectRatio **aBaseVal)
      { return mVal->ToDOMBaseVal(aBaseVal, mSVGElement); }

    NS_IMETHOD GetAnimVal(nsIDOMSVGPreserveAspectRatio **aAnimVal)
      { return mVal->ToDOMAnimVal(aAnimVal, mSVGElement); }
  };

};

#endif //__NS_SVGPRESERVEASPECTRATIO_H__
