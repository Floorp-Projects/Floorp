/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsIDOMHTMLObjectElement.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsDOMError.h"


class nsHTMLObjectElement : public nsGenericHTMLContainerElement,
                            public nsIDOMHTMLObjectElement
{
public:
  nsHTMLObjectElement();
  virtual ~nsHTMLObjectElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_IDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_IDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_IDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLObjectElement
  NS_DECL_IDOMHTMLOBJECTELEMENT

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAWritableString& aResult) const;
  NS_IMETHOD GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc, 
                                          nsMapAttributesFunc& aMapFunc) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                      PRInt32& aHint) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;

  // nsIScriptObjectOwner
  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext,
                             void** aScriptObject);

  // nsIJSScriptObject
  virtual PRBool GetProperty(JSContext *aContext, JSObject *aObj, 
                             jsval aID, jsval *aVp);
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

NS_IMPL_HTMLCONTENT_QI(nsHTMLObjectElement, nsGenericHTMLContainerElement,
                       nsIDOMHTMLObjectElement);


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
  *aForm = nsnull;/* XXX */
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
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Hspace, hspace)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Standby, standby)
NS_IMPL_INT_ATTR(nsHTMLObjectElement, TabIndex, tabindex)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Type, type)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, UseMap, usemap)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Vspace, vspace)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Width, width)


NS_IMETHODIMP
nsHTMLObjectElement::GetContentDocument(nsIDOMDocument** aContentDocument)
{
  NS_ENSURE_ARG_POINTER(aContentDocument);
  *aContentDocument = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLObjectElement::SetContentDocument(nsIDOMDocument* aContentDocument)
{
  return NS_ERROR_DOM_INVALID_MODIFICATION_ERR;
}

NS_IMETHODIMP
nsHTMLObjectElement::StringToAttribute(nsIAtom* aAttribute,
                                       const nsAReadableString& aValue,
                                       nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (ParseAlignValue(aValue, aResult)) {
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
                                       nsAWritableString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      AlignValueToString(aValue, aResult);
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
MapAttributesInto(const nsIHTMLMappedAttributes* aAttributes,
                  nsIMutableStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  nsGenericHTMLElement::MapImageAlignAttributeInto(aAttributes, aContext,
                                                   aPresContext);
  nsGenericHTMLElement::MapImageAttributesInto(aAttributes, aContext,
                                               aPresContext);
  nsGenericHTMLElement::MapImageBorderAttributeInto(aAttributes, aContext,
                                                    aPresContext, nsnull);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext,
                                                aPresContext);
}

NS_IMETHODIMP
nsHTMLObjectElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                              PRInt32& aHint) const
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
nsHTMLObjectElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                  nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLObjectElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLObjectElement::GetScriptObject(nsIScriptContext* aContext,
                                     void** aScriptObject)
{
  return GetPluginScriptObject(aContext, aScriptObject);
}

PRBool
nsHTMLObjectElement::GetProperty(JSContext *aContext, JSObject *aObj,
                                 jsval aID, jsval *aVp)
{
  return GetPluginProperty(aContext, aObj, aID, aVp);
}
