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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corporation
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

#ifndef __NS_SVGSTYLABLEELEMENT_H__
#define __NS_SVGSTYLABLEELEMENT_H__

#include "nsSVGElement.h"
#include "nsIDOMSVGStylable.h"
#include "nsIDOMSVGAnimatedString.h"
#include "nsAutoPtr.h"

typedef nsSVGElement nsSVGStylableElementBase;

class nsSVGStylableElement : public nsSVGStylableElementBase,
                             public nsIDOMSVGStylable
{
protected:
  nsSVGStylableElement(nsINodeInfo *aNodeInfo);

public:
  // interfaces:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGSTYLABLE

  // nsIContent
  virtual const nsAttrValue* DoGetClasses() const;

  // nsSVGElement
  virtual nsresult UnsetAttr(PRInt32 aNamespaceID, nsIAtom* aAttribute,
                             PRBool aNotify);

protected:

  // nsSVGElement
  virtual PRBool ParseAttribute(PRInt32 aNamespaceID, nsIAtom* aName,
                                const nsAString& aValue,
                                nsAttrValue& aResult);

private:
  nsAutoPtr<nsAttrValue> mClassAnimAttr;

  const nsAttrValue* GetClassAnimAttr() const;
 
  void GetClassBaseValString(nsAString &aResult) const;
  void SetClassBaseValString(const nsAString& aValue);
  void GetClassAnimValString(nsAString& aResult) const;

  struct DOMAnimatedClassString : public nsIDOMSVGAnimatedString
  {
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(DOMAnimatedClassString)

    DOMAnimatedClassString(nsSVGStylableElement *aSVGElement)
      : mSVGElement(aSVGElement) {}

    nsRefPtr<nsSVGStylableElement> mSVGElement;

    NS_IMETHOD GetBaseVal(nsAString& aResult)
      { mSVGElement->GetClassBaseValString(aResult); return NS_OK; }
    NS_IMETHOD SetBaseVal(const nsAString& aValue)
      { mSVGElement->SetClassBaseValString(aValue); return NS_OK; }
    NS_IMETHOD GetAnimVal(nsAString& aResult)
      { mSVGElement->GetClassAnimValString(aResult); return NS_OK; }
  };
};


#endif // __NS_SVGSTYLABLEELEMENT_H__
