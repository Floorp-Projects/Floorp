/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsIDOMHTMLObjectElement.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsDOMError.h"

#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIDOMDocument.h"
#include "nsIWebNavigation.h"
#include "nsIFormControl.h"

class nsHTMLObjectElement : public nsGenericHTMLContainerFormElement,
                            public nsIDOMHTMLObjectElement
{
public:
  nsHTMLObjectElement();
  virtual ~nsHTMLObjectElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLObjectElement
  NS_DECL_NSIDOMHTMLOBJECTELEMENT

  // Overriden nsIFormControl methods
  NS_IMETHOD_(PRInt32) GetType() { return NS_FORM_OBJECT; }
  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                               nsIContent* aSubmitElement);
  NS_IMETHOD SaveState();
  NS_IMETHOD RestoreState(nsIPresState* aState);

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAString& aResult) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                      nsChangeHint& aHint) const;
#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif
};

nsresult
NS_NewHTMLObjectElement(nsIHTMLContent** aInstancePtrResult,
                        nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLObjectElement* it = new nsHTMLObjectElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = it->Init(aNodeInfo);

  if (NS_FAILED(rv)) {
    delete it;

    return rv;
  }

  *aInstancePtrResult = NS_STATIC_CAST(nsIHTMLContent *, it);
  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}


nsHTMLObjectElement::nsHTMLObjectElement()
{
}

nsHTMLObjectElement::~nsHTMLObjectElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLObjectElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLObjectElement, nsGenericElement) 

// QueryInterface implementation for nsHTMLObjectElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLObjectElement,
                                    nsGenericHTMLContainerFormElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLObjectElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLObjectElement)
NS_HTML_CONTENT_INTERFACE_MAP_END

// nsIDOMHTMLObjectElement
nsresult
nsHTMLObjectElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLObjectElement* it = new nsHTMLObjectElement();

  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIDOMNode> kungFuDeathGrip(it);

  nsresult rv = it->Init(mNodeInfo);

  if (NS_FAILED(rv))
    return rv;

  CopyInnerTo(this, it, aDeep);

  *aReturn = NS_STATIC_CAST(nsIDOMNode *, it);

  NS_ADDREF(*aReturn);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLObjectElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLContainerFormElement::GetForm(aForm);
}

// nsIFormControl

NS_IMETHODIMP
nsHTMLObjectElement::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLObjectElement::SubmitNamesValues(nsIFormSubmission* aFormSubmission,
                                       nsIContent* aSubmitElement)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLObjectElement::SaveState()
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLObjectElement::RestoreState(nsIPresState* aState)
{
  return NS_OK;
}

NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Code, code)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Archive, archive)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Border, border)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, CodeBase, codebase)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, CodeType, codetype)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Data, data)
NS_IMPL_BOOL_ATTR(nsHTMLObjectElement, Declare, declare)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Height, height)
NS_IMPL_PIXEL_ATTR(nsHTMLObjectElement, Hspace, hspace)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Standby, standby)
NS_IMPL_INT_ATTR(nsHTMLObjectElement, TabIndex, tabindex)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Type, type)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, UseMap, usemap)
NS_IMPL_PIXEL_ATTR(nsHTMLObjectElement, Vspace, vspace)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Width, width)


NS_IMETHODIMP
nsHTMLObjectElement::GetContentDocument(nsIDOMDocument** aContentDocument)
{
  NS_ENSURE_ARG_POINTER(aContentDocument);

  *aContentDocument = nsnull;

  if (!mDocument) {
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> sub_doc;
  mDocument->GetSubDocumentFor(this, getter_AddRefs(sub_doc));

  if (!sub_doc) {
    return NS_OK;
  }

  return CallQueryInterface(sub_doc, aContentDocument);
}

NS_IMETHODIMP
nsHTMLObjectElement::SetContentDocument(nsIDOMDocument* aContentDocument)
{
  return NS_ERROR_DOM_INVALID_MODIFICATION_ERR;
}

NS_IMETHODIMP
nsHTMLObjectElement::StringToAttribute(nsIAtom* aAttribute,
                                       const nsAString& aValue,
                                       nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (ParseAlignValue(aValue, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (aAttribute == nsHTMLAtoms::tabindex) {
    if (aResult.ParseIntWithBounds(aValue, eHTMLUnit_Integer, 0)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (ParseImageAttribute(aAttribute, aValue, aResult)) {
    return NS_CONTENT_ATTR_HAS_VALUE;
  }

  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLObjectElement::AttributeToString(nsIAtom* aAttribute,
                                       const nsHTMLValue& aValue,
                                       nsAString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      VAlignValueToString(aValue, aResult);
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  else if (ImageAttributeToString(aAttribute, aValue, aResult)) {
    return NS_CONTENT_ATTR_HAS_VALUE;
  }

  return nsGenericHTMLContainerElement::AttributeToString(aAttribute, aValue,
                                                          aResult);
}

static void
MapAttributesIntoRule(const nsIHTMLMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  if (!aData)
    return;

  nsGenericHTMLElement::MapAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageBorderAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageMarginAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImagePositionAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP
nsHTMLObjectElement::GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                              nsChangeHint& aHint) const
{
  if (!GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    if (!GetImageBorderAttributeImpact(aAttribute, aHint)) {
      if (!GetImageMappedAttributesImpact(aAttribute, aHint)) {
        if (!GetImageAlignAttributeImpact(aAttribute, aHint)) {
          aHint = NS_STYLE_HINT_CONTENT;
        }
      }
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsHTMLObjectElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLObjectElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
#endif
