/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGCLASS_H__
#define __NS_SVGCLASS_H__

#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDOMError.h"
#include "nsIDOMSVGAnimatedString.h"
#include "nsISMILAttr.h"
#include "nsString.h"

class nsSVGStylableElement;

class nsSVGClass
{

public:
  void Init() {
    mAnimVal = nsnull;
  }

  void SetBaseValue(const nsAString& aValue,
                    nsSVGStylableElement *aSVGElement,
                    bool aDoSetAttr);
  void GetBaseValue(nsAString& aValue, const nsSVGStylableElement *aSVGElement) const;

  void SetAnimValue(const nsAString& aValue, nsSVGStylableElement *aSVGElement);
  void GetAnimValue(nsAString& aValue, const nsSVGStylableElement *aSVGElement) const;
  bool IsAnimated() const
    { return !!mAnimVal; }

  nsresult ToDOMAnimatedString(nsIDOMSVGAnimatedString **aResult,
                               nsSVGStylableElement *aSVGElement);
  // Returns a new nsISMILAttr object that the caller must delete
  nsISMILAttr* ToSMILAttr(nsSVGStylableElement *aSVGElement);

private:

  nsAutoPtr<nsString> mAnimVal;

public:
  struct DOMAnimatedString : public nsIDOMSVGAnimatedString
  {
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(DOMAnimatedString)

    DOMAnimatedString(nsSVGClass *aVal, nsSVGStylableElement *aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}

    nsSVGClass* mVal; // kept alive because it belongs to content
    nsRefPtr<nsSVGStylableElement> mSVGElement;

    NS_IMETHOD GetBaseVal(nsAString& aResult)
      { mVal->GetBaseValue(aResult, mSVGElement); return NS_OK; }
    NS_IMETHOD SetBaseVal(const nsAString& aValue)
      { mVal->SetBaseValue(aValue, mSVGElement, true); return NS_OK; }

    NS_IMETHOD GetAnimVal(nsAString& aResult);
  };
  struct SMILString : public nsISMILAttr
  {
  public:
    SMILString(nsSVGClass *aVal, nsSVGStylableElement *aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}

    // These will stay alive because a nsISMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    nsSVGClass* mVal;
    nsSVGStylableElement* mSVGElement;

    // nsISMILAttr methods
    virtual nsresult ValueFromString(const nsAString& aStr,
                                     const nsISMILAnimationElement *aSrcElement,
                                     nsSMILValue& aValue,
                                     bool& aPreventCachingOfSandwich) const;
    virtual nsSMILValue GetBaseValue() const;
    virtual void ClearAnimValue();
    virtual nsresult SetAnimValue(const nsSMILValue& aValue);
  };
};
#endif //__NS_SVGCLASS_H__
