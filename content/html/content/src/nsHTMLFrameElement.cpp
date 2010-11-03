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
#include "nsGenericHTMLElement.h"
#include "nsGkAtoms.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsDOMError.h"

using namespace mozilla::dom;

class nsHTMLFrameElement : public nsGenericHTMLFrameElement,
                           public nsIDOMHTMLFrameElement
{
public:
  nsHTMLFrameElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                     mozilla::dom::FromParser aFromParser = mozilla::dom::NOT_FROM_PARSER);
  virtual ~nsHTMLFrameElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLFrameElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFrameElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLFrameElement::)

  // nsIDOMHTMLFrameElement
  NS_DECL_NSIDOMHTMLFRAMEELEMENT

  // nsIContent
  virtual PRBool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;
  nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;
  virtual nsXPCClassInfo* GetClassInfo();
};


NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Frame)


nsHTMLFrameElement::nsHTMLFrameElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                                       FromParser aFromParser)
  : nsGenericHTMLFrameElement(aNodeInfo, aFromParser)
{
}

nsHTMLFrameElement::~nsHTMLFrameElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLFrameElement, nsGenericElement)
NS_IMPL_RELEASE_INHERITED(nsHTMLFrameElement, nsGenericElement)


DOMCI_NODE_DATA(HTMLFrameElement, nsHTMLFrameElement)

// QueryInterface implementation for nsHTMLFrameElement
NS_INTERFACE_TABLE_HEAD(nsHTMLFrameElement)
  NS_HTML_CONTENT_INTERFACE_TABLE1(nsHTMLFrameElement, nsIDOMHTMLFrameElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLFrameElement,
                                               nsGenericHTMLFrameElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLFrameElement)


NS_IMPL_ELEMENT_CLONE(nsHTMLFrameElement)


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
nsHTMLFrameElement::ParseAttribute(PRInt32 aNamespaceID,
                                   nsIAtom* aAttribute,
                                   const nsAString& aValue,
                                   nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::bordercolor) {
      return aResult.ParseColor(aValue);
    }
    if (aAttribute == nsGkAtoms::frameborder) {
      return ParseFrameborderValue(aValue, aResult);
    }
    if (aAttribute == nsGkAtoms::marginwidth) {
      return aResult.ParseSpecialIntValue(aValue, PR_TRUE);
    }
    if (aAttribute == nsGkAtoms::marginheight) {
      return aResult.ParseSpecialIntValue(aValue, PR_TRUE);
    }
    if (aAttribute == nsGkAtoms::scrolling) {
      return ParseScrollingValue(aValue, aResult);
    }
  }

  return nsGenericHTMLFrameElement::ParseAttribute(aNamespaceID, aAttribute,
                                                   aValue, aResult);
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

nsMapRuleToAttributesFunc
nsHTMLFrameElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}
