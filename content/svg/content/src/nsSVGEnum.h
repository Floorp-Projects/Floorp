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
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
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

#ifndef __NS_SVGENUM_H__
#define __NS_SVGENUM_H__

#include "nsIDOMSVGAnimatedEnum.h"
#include "nsSVGElement.h"
#include "nsDOMError.h"

typedef PRUint8 nsSVGEnumValue;

struct nsSVGEnumMapping {
  nsIAtom **mKey;
  nsSVGEnumValue mVal;
};

class nsSVGEnum
{
public:
  void Init(PRUint8 aAttrEnum, PRUint16 aValue) {
    mAnimVal = mBaseVal = static_cast<PRUint8>(aValue);
    mAttrEnum = aAttrEnum;
  }

  nsresult SetBaseValueString(const nsAString& aValue,
                              nsSVGElement *aSVGElement,
                              PRBool aDoSetAttr);
  void GetBaseValueString(nsAString& aValue,
                          nsSVGElement *aSVGElement);

  void SetBaseValue(PRUint16 aValue,
                    nsSVGElement *aSVGElement,
                    PRBool aDoSetAttr);

  PRUint16 GetBaseValue() const
    { return mBaseVal; }
  PRUint16 GetAnimValue() const
    { return mAnimVal; }

  nsresult ToDOMAnimatedEnum(nsIDOMSVGAnimatedEnumeration **aResult,
                             nsSVGElement* aSVGElement);

private:
  nsSVGEnumValue mAnimVal;
  nsSVGEnumValue mBaseVal;
  PRUint8 mAttrEnum; // element specified tracking for attribute

  nsSVGEnumMapping *GetMapping(nsSVGElement *aSVGElement);

  struct DOMAnimatedEnum : public nsIDOMSVGAnimatedEnumeration
  {
    NS_DECL_ISUPPORTS

    DOMAnimatedEnum(nsSVGEnum* aVal, nsSVGElement *aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}

    nsSVGEnum *mVal; // kept alive because it belongs to content
    nsRefPtr<nsSVGElement> mSVGElement;

    NS_IMETHOD GetBaseVal(PRUint16* aResult)
      { *aResult = mVal->GetBaseValue(); return NS_OK; }
    NS_IMETHOD SetBaseVal(PRUint16 aValue)
      { mVal->SetBaseValue(aValue, mSVGElement, PR_TRUE); return NS_OK; }
    NS_IMETHOD GetAnimVal(PRUint16* aResult)
      { *aResult = mVal->GetAnimValue(); return NS_OK; }
  };
};

#endif //__NS_SVGENUM2_H__
