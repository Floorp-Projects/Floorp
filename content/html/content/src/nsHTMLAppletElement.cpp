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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "nsIDOMHTMLAppletElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsIDocument.h"
#include "nsLayoutAtoms.h"

// XXX this is to get around conflicts with windows.h defines
// introduced through jni.h
#ifdef XP_WIN
#undef GetClassName
#undef GetObject
#endif


class nsHTMLAppletElement : public nsGenericHTMLElement,
                            public nsIDOMHTMLAppletElement
{
public:
  nsHTMLAppletElement(nsINodeInfo *aNodeInfo, PRBool aFromParser = PR_FALSE);
  virtual ~nsHTMLAppletElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLAppletElement
  NS_DECL_NSIDOMHTMLAPPLETELEMENT

  virtual void DoneAddingChildren();
  virtual PRBool IsDoneAddingChildren();

  virtual PRBool ParseAttribute(nsIAtom* aAttribute,
                                const nsAString& aValue,
                                nsAttrValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAString& aResult) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom* aAttribute) const;

protected:
  PRPackedBool mReflectedApplet;
  PRPackedBool mIsDoneAddingChildren;
};


NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Applet)


nsHTMLAppletElement::nsHTMLAppletElement(nsINodeInfo *aNodeInfo, PRBool aFromParser)
  : nsGenericHTMLElement(aNodeInfo), mReflectedApplet(PR_FALSE),
    mIsDoneAddingChildren(!aFromParser)
{
}

nsHTMLAppletElement::~nsHTMLAppletElement()
{
}

PRBool
nsHTMLAppletElement::IsDoneAddingChildren()
{
  return mIsDoneAddingChildren;
}

void
nsHTMLAppletElement::DoneAddingChildren()
{
  mIsDoneAddingChildren = PR_TRUE;
  RecreateFrames();  
}

NS_IMPL_ADDREF_INHERITED(nsHTMLAppletElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLAppletElement, nsGenericElement) 


// QueryInterface implementation for nsHTMLAppletElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLAppletElement, nsGenericHTMLElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLAppletElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLAppletElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


NS_IMPL_HTML_DOM_CLONENODE(Applet)


NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Alt, alt)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Archive, archive)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Code, code)
NS_IMPL_URI_ATTR(nsHTMLAppletElement, CodeBase, codebase)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Height, height)
NS_IMPL_INT_ATTR(nsHTMLAppletElement, Hspace, hspace)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Object, object)
NS_IMPL_INT_ATTR(nsHTMLAppletElement, Vspace, vspace)
NS_IMPL_STRING_ATTR(nsHTMLAppletElement, Width, width)

PRBool
nsHTMLAppletElement::ParseAttribute(nsIAtom* aAttribute,
                                    const nsAString& aValue,
                                    nsAttrValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::align) {
    return nsGenericHTMLElement::ParseAlignValue(aValue, aResult);
  }
  if (nsGenericHTMLElement::ParseImageAttribute(aAttribute,
                                                aValue, aResult)) {
    return PR_TRUE;
  }

  return nsGenericHTMLElement::ParseAttribute(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLAppletElement::AttributeToString(nsIAtom* aAttribute,
                                       const nsHTMLValue& aValue,
                                       nsAString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      nsGenericHTMLElement::VAlignValueToString(aValue, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return nsGenericHTMLElement::AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  nsGenericHTMLElement::MapImageBorderAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageMarginAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageSizeAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(PRBool)
nsHTMLAppletElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sCommonAttributeMap,
    sImageMarginSizeAttributeMap,
    sImageAlignAttributeMap,
    sImageBorderAttributeMap
  };
  
  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
}

NS_IMETHODIMP
nsHTMLAppletElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;
  return NS_OK;
}
