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
#include "nsIDOMHTMLTableCaptionElem.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIHTMLAttributes.h"
#include "nsIRuleNode.h"

class nsHTMLTableCaptionElement :  public nsGenericHTMLContainerElement,
                                   public nsIDOMHTMLTableCaptionElement
{
public:
  nsHTMLTableCaptionElement();
  virtual ~nsHTMLTableCaptionElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_NO_CLONENODE(nsGenericHTMLContainerElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLContainerElement::)

  // nsIDOMHTMLTableCaptionElement
  NS_DECL_NSIDOMHTMLTABLECAPTIONELEMENT

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsAReadableString& aValue,
                               nsHTMLValue& aResult);
  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               const nsHTMLValue& aValue,
                               nsAWritableString& aResult) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const;
  NS_IMETHOD GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                      PRInt32& aHint) const;
#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif
};

nsresult
NS_NewHTMLTableCaptionElement(nsIHTMLContent** aInstancePtrResult,
                              nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsHTMLTableCaptionElement* it = new nsHTMLTableCaptionElement();

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


nsHTMLTableCaptionElement::nsHTMLTableCaptionElement()
{
}

nsHTMLTableCaptionElement::~nsHTMLTableCaptionElement()
{
}


NS_IMPL_ADDREF_INHERITED(nsHTMLTableCaptionElement, nsGenericElement);
NS_IMPL_RELEASE_INHERITED(nsHTMLTableCaptionElement, nsGenericElement);


// QueryInterface implementation for nsHTMLTableCaptionElement
NS_HTML_CONTENT_INTERFACE_MAP_BEGIN(nsHTMLTableCaptionElement,
                                    nsGenericHTMLContainerElement)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLTableCaptionElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLTableCaptionElement)
NS_HTML_CONTENT_INTERFACE_MAP_END


nsresult
nsHTMLTableCaptionElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsHTMLTableCaptionElement* it = new nsHTMLTableCaptionElement();

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


NS_IMPL_STRING_ATTR(nsHTMLTableCaptionElement, Align, align)


static nsGenericHTMLElement::EnumTable kCaptionAlignTable[] = {
  { "left",  NS_SIDE_LEFT },
  { "right", NS_SIDE_RIGHT },
  { "top",   NS_SIDE_TOP},
  { "bottom",NS_SIDE_BOTTOM},
  { 0 }
};

NS_IMETHODIMP
nsHTMLTableCaptionElement::StringToAttribute(nsIAtom* aAttribute,
                                      const nsAReadableString& aValue,
                                      nsHTMLValue&    aResult)
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (ParseEnumValue(aValue, kCaptionAlignTable, aResult)) {
      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLTableCaptionElement::AttributeToString(nsIAtom* aAttribute,
                                      const nsHTMLValue& aValue,
                                      nsAWritableString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::align) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      EnumValueToString(aValue, kCaptionAlignTable, aResult);

      return NS_CONTENT_ATTR_HAS_VALUE;
    }
  }

  return nsGenericHTMLElement::AttributeToString(aAttribute, aValue, aResult);
}

static 
void MapAttributesIntoRule(const nsIHTMLMappedAttributes* aAttributes, nsRuleData* aData)
{
  if (!aAttributes || !aData || aData->mSID != eStyleStruct_TableBorder || !aData->mTableData)
    return;

  if (aData->mTableData->mCaptionSide.GetUnit() == eCSSUnit_Null) {
    nsHTMLValue value;
    aAttributes->GetAttribute(nsHTMLAtoms::align, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated)
      aData->mTableData->mCaptionSide.SetIntValue(value.GetIntValue(), eCSSUnit_Enumerated);
  }

  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP
nsHTMLTableCaptionElement::GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType,
                                                    PRInt32& aHint) const
{
  if (aAttribute == nsHTMLAtoms::align) {
    aHint = NS_STYLE_HINT_REFLOW;
  }
  else if (!GetCommonMappedAttributesImpact(aAttribute, aHint)) {
    aHint = NS_STYLE_HINT_CONTENT;
  }

  return NS_OK;
}



NS_IMETHODIMP
nsHTMLTableCaptionElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  aMapRuleFunc = &MapAttributesIntoRule;
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsHTMLTableCaptionElement::SizeOf(nsISizeOfHandler* aSizer,
                                  PRUint32* aResult) const
{
  *aResult = sizeof(*this) + BaseSizeOf(aSizer);

  return NS_OK;
}
#endif
