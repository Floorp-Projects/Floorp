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
 * The Initial Developer of the Original Code is
 * Scooter Morris.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scooter Morris <scootermorris@comcast.net>
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

#include "nsSVGStylableElement.h"
#include "nsIDOMSVGStopElement.h"
#include "nsSVGAnimatedNumberList.h"
#include "nsSVGNumber2.h"
#include "nsGenericHTMLElement.h"
#include "prdtoa.h"

typedef nsSVGStylableElement nsSVGStopElementBase;

class nsSVGStopElement : public nsSVGStopElementBase,
                         public nsIDOMSVGStopElement
{
protected:
  friend nsresult NS_NewSVGStopElement(nsIContent **aResult,
                                       nsINodeInfo *aNodeInfo);
  nsSVGStopElement(nsINodeInfo* aNodeInfo);

public:
  // interfaces:

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMSVGSTOPELEMENT

  // xxx If xpcom allowed virtual inheritance we wouldn't need to
  // forward here :-(
  NS_FORWARD_NSIDOMNODE(nsSVGStopElementBase::)
  NS_FORWARD_NSIDOMELEMENT(nsSVGStopElementBase::)
  NS_FORWARD_NSIDOMSVGELEMENT(nsSVGStopElementBase::)

  // nsIContent interface
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  PRBool ParseAttribute(PRInt32 aNamespaceID, nsIAtom* aAttribute,
                        const nsAString& aValue, nsAttrValue& aResult);

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

protected:

  virtual NumberAttributesInfo GetNumberInfo();
  // nsIDOMSVGStopElement properties:
  nsSVGNumber2 mOffset;
  static NumberInfo sNumberInfo;
};

nsSVGElement::NumberInfo nsSVGStopElement::sNumberInfo = { &nsGkAtoms::offset, 
                                                           0 };
NS_IMPL_NS_NEW_SVG_ELEMENT(Stop)

//----------------------------------------------------------------------
// nsISupports methods

NS_IMPL_ADDREF_INHERITED(nsSVGStopElement,nsSVGStopElementBase)
NS_IMPL_RELEASE_INHERITED(nsSVGStopElement,nsSVGStopElementBase)

NS_INTERFACE_MAP_BEGIN(nsSVGStopElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGStopElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(SVGStopElement)
NS_INTERFACE_MAP_END_INHERITING(nsSVGStopElementBase)

//----------------------------------------------------------------------
// Implementation

nsSVGStopElement::nsSVGStopElement(nsINodeInfo* aNodeInfo)
  : nsSVGStopElementBase(aNodeInfo)
{

}

//----------------------------------------------------------------------
// nsIDOMNode methods

NS_IMPL_ELEMENT_CLONE_WITH_INIT(nsSVGStopElement)

//----------------------------------------------------------------------
// nsIDOMSVGStopElement methods

/* readonly attribute nsIDOMSVGAnimatedLengthList x; */
NS_IMETHODIMP nsSVGStopElement::GetOffset(nsIDOMSVGAnimatedNumber * *aOffset)
{
  return mOffset.ToDOMAnimatedNumber(aOffset,this);
}

//----------------------------------------------------------------------
// nsSVGElement methods

nsSVGElement::NumberAttributesInfo
nsSVGStopElement::GetNumberInfo()
{
  return NumberAttributesInfo(&mOffset, &sNumberInfo, 1);
}

PRBool
nsSVGStopElement::ParseAttribute(PRInt32 aNamespaceID,
								 nsIAtom* aAttribute,
								 const nsAString& aValue,
								 nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::offset) {
      NS_ConvertUTF16toUTF8 value(aValue);
      const char *str = value.get();

      char *rest;
      float offset = NS_STATIC_CAST(float, PR_strtod(str, &rest));
      if (str != rest) {
        if (*rest == '%') {
          offset /= 100;
          ++rest;
        }
        if (*rest == '\0') {
          mOffset.SetBaseValue(offset, this, PR_FALSE);
          aResult.SetTo(aValue);
          return PR_TRUE;
        }
      }
    }
  }
  return nsSVGElement::ParseAttribute(aNamespaceID, aAttribute,
                                      aValue, aResult);
}

//----------------------------------------------------------------------
// nsIContent methods

NS_IMETHODIMP_(PRBool)
nsSVGStopElement::IsAttributeMapped(const nsIAtom* name) const
{
  static const MappedAttributeEntry* const map[] = {
    sGradientStopMap
  };
  
  return FindAttributeDependence(name, map, NS_ARRAY_LENGTH(map)) ||
    nsSVGStopElementBase::IsAttributeMapped(name);
}


