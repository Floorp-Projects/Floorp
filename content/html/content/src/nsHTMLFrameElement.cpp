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
#include "nsIDOMHTMLFrameElement.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsIPresShell.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsDOMError.h"


class nsHTMLFrameElement : public nsGenericHTMLFrameElement,
                           public nsIDOMHTMLFrameElement
{
public:
  nsHTMLFrameElement(nsINodeInfo *aNodeInfo);
  virtual ~nsHTMLFrameElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLFrameElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFrameElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLFrameElement::)

  // nsIDOMHTMLFrameElement
  NS_DECL_NSIDOMHTMLFRAMEELEMENT

  // nsIContent
  virtual PRBool ParseAttribute(nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAString& aResult) const;
  
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
};


NS_IMPL_NS_NEW_HTML_ELEMENT(Frame)


nsHTMLFrameElement::nsHTMLFrameElement(nsINodeInfo *aNodeInfo)
  : nsGenericHTMLFrameElement(aNodeInfo)
{
}

nsHTMLFrameElement::~nsHTMLFrameElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLFrameElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLFrameElement, nsGenericElement)


// QueryInterface implementation for nsHTMLFrameElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLFrameElement,
                                    nsGenericHTMLFrameElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLFrameElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLFrameElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_HTML_DOM_CLONENODE(Frame)


NS_IMPL_STRING_ATTR(nsHTMLFrameElement, FrameBorder, frameborder)
NS_IMPL_URI_ATTR(nsHTMLFrameElement, LongDesc, longdesc)
NS_IMPL_STRING_ATTR(nsHTMLFrameElement, MarginHeight, marginheight)
NS_IMPL_STRING_ATTR(nsHTMLFrameElement, MarginWidth, marginwidth)
NS_IMPL_STRING_ATTR(nsHTMLFrameElement, Name, name)
NS_IMPL_BOOL_ATTR(nsHTMLFrameElement, NoResize, noresize)
NS_IMPL_STRING_ATTR(nsHTMLFrameElement, Scrolling, scrolling)
NS_IMPL_URI_ATTR(nsHTMLFrameElement, Src, src)


NS_IMETHODIMP
nsHTMLFrameElement::GetContentDocument(nsIDOMDocument** aContentDocument)
{
  return nsGenericHTMLFrameElement::GetContentDocument(aContentDocument);
}

PRBool
nsHTMLFrameElement::ParseAttribute(nsIAtom* aAttribute,
                                   const nsAString& aValue,
                                   nsAttrValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::bordercolor) {
    return aResult.ParseColor(aValue, nsGenericHTMLFrameElement::GetOwnerDoc());
  }
  if (aAttribute == nsHTMLAtoms::frameborder) {
    return ParseFrameborderValue(aValue, aResult);
  }
  if (aAttribute == nsHTMLAtoms::marginwidth) {
    return aResult.ParseSpecialIntValue(aValue, PR_TRUE, PR_FALSE);
  }
  if (aAttribute == nsHTMLAtoms::marginheight) {
    return aResult.ParseSpecialIntValue(aValue, PR_TRUE, PR_FALSE);
  }
  if (aAttribute == nsHTMLAtoms::scrolling) {
    return ParseScrollingValue(aValue, aResult);
  }

  return nsGenericHTMLFrameElement::ParseAttribute(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLFrameElement::AttributeToString(nsIAtom* aAttribute,
                                      const nsHTMLValue& aValue,
                                      nsAString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::frameborder) {
    FrameborderValueToString(aValue, aResult);
    return NS_CONTENT_ATTR_HAS_VALUE;
  } 
  else if (aAttribute == nsHTMLAtoms::scrolling) {
    ScrollingValueToString(aValue, aResult);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }

  return nsGenericHTMLFrameElement::AttributeToString(aAttribute, aValue,
                                                      aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  nsGenericHTMLElement::MapScrollingAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(PRBool)
nsHTMLFrameElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sScrollingAttributeMap,
    sCommonAttributeMap,
  };
  
  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
}

NS_IMETHODIMP
nsHTMLFrameElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;
  return NS_OK;
}
