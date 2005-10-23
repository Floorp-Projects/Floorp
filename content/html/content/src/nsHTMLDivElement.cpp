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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#include "nsIDOMHTMLDivElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsMappedAttributes.h"

class nsHTMLDivElement : public nsGenericHTMLElement,
                         public nsIDOMHTMLDivElement
{
public:
  nsHTMLDivElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLDivElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLDivElement
  NS_DECL_NSIDOMHTMLDIVELEMENT

  virtual PRBool ParseAttribute(nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Div)


nsHTMLDivElement::nsHTMLDivElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLElement(aNodeInfo)
{
}

nsHTMLDivElement::~nsHTMLDivElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLDivElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLDivElement, nsGenericElement) 



// QueryInterface implementation for nsHTMLDivElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLDivElement, nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLDivElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLDivElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_DOM_CLONENODE(nsHTMLDivElement)


NS_IMPL_STRING_ATTR(nsHTMLDivElement, Align, align)


PRBool
nsHTMLDivElement::ParseAttribute(nsIAtom* aAttribute,
                                 const nsAString& aValue,
                                 nsAttrValue& aResult)
{
  if (mNodeInfo->Equals(nsHTMLAtoms::marquee)) {
   if ((aAttribute == nsHTMLAtoms::width) ||
       (aAttribute == nsHTMLAtoms::height)) {
     return aResult.ParseSpecialIntValue(aValue, PR_TRUE, PR_FALSE);
   }
   else if ((aAttribute == nsHTMLAtoms::hspace) ||
            (aAttribute == nsHTMLAtoms::vspace)) {
     return aResult.ParseIntWithBounds(aValue, 0);
   }
  }

  if (mNodeInfo->Equals(nsHTMLAtoms::div) && aAttribute == nsHTMLAtoms::align) {
    return ParseDivAlignValue(aValue, aResult);
  }

  return nsGenericHTMLElement::ParseAttribute(aAttribute, aValue, aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes, nsRuleData* aData)
{
  nsGenericHTMLElement::MapDivAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

static void
MapMarqueeAttributesIntoRule(const nsMappedAttributes* aAttributes, nsRuleData* aData)
{
  nsGenericHTMLElement::MapImageMarginAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageSizeAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(PRBool)
nsHTMLDivElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  if (mNodeInfo->Equals(nsHTMLAtoms::div)) {
    static const MappedAttributeEntry* const map[] = {
      sDivAlignAttributeMap,
      sCommonAttributeMap
    };
    return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
  }
  if (mNodeInfo->Equals(nsHTMLAtoms::marquee)) {  
    static const MappedAttributeEntry* const map[] = {
      sImageMarginSizeAttributeMap,
      sCommonAttributeMap
    };
    return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
  }

  return nsGenericHTMLElement::IsAttributeMapped(aAttribute);
}

nsMapRuleToAttributesFunc
nsHTMLDivElement::GetAttributeMappingFunction() const
{
  if (mNodeInfo->Equals(nsHTMLAtoms::div)) {
    return &MapAttributesIntoRule;
  }
  if (mNodeInfo->Equals(nsHTMLAtoms::marquee)) {
    return &MapMarqueeAttributesIntoRule;
  }  
  return nsGenericHTMLElement::GetAttributeMappingFunction();
}
