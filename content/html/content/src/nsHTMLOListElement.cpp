/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsIDOMHTMLOListElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIHTMLAttributes.h"

static NS_DEFINE_IID(kIDOMHTMLOListElementIID, NS_IDOMHTMLOLISTELEMENT_IID);

class nsHTMLOListElement : public nsIDOMHTMLOListElement,
                           public nsIScriptObjectOwner,
                           public nsIDOMEventReceiver,
                           public nsIHTMLContent
{
public:
  nsHTMLOListElement(nsIAtom* aTag);
  virtual ~nsHTMLOListElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLOListElement
  NS_IMETHOD GetCompact(PRBool* aCompact);
  NS_IMETHOD SetCompact(PRBool aCompact);
  NS_IMETHOD GetStart(PRInt32* aStart);
  NS_IMETHOD SetStart(PRInt32 aStart);
  NS_IMETHOD GetType(nsString& aType);
  NS_IMETHOD SetType(const nsString& aType);

  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

protected:
  nsGenericHTMLContainerElement mInner;
};

nsresult
NS_NewHTMLOListElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLOListElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLOListElement::nsHTMLOListElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLOListElement::~nsHTMLOListElement()
{
}

NS_IMPL_ADDREF(nsHTMLOListElement)

NS_IMPL_RELEASE(nsHTMLOListElement)

nsresult
nsHTMLOListElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLOListElementIID)) {
    nsIDOMHTMLOListElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLOListElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLOListElement* it = new nsHTMLOListElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMPL_BOOL_ATTR(nsHTMLOListElement, Compact, compact)
NS_IMPL_INT_ATTR(nsHTMLOListElement, Start, start)
NS_IMPL_STRING_ATTR(nsHTMLOListElement, Type, type)

nsGenericHTMLElement::EnumTable kListTypeTable[] = {
  { "none", NS_STYLE_LIST_STYLE_NONE },
  { "disc", NS_STYLE_LIST_STYLE_DISC },
  { "circle", NS_STYLE_LIST_STYLE_CIRCLE },
  { "round", NS_STYLE_LIST_STYLE_CIRCLE },
  { "square", NS_STYLE_LIST_STYLE_SQUARE },
  { "decimal", NS_STYLE_LIST_STYLE_DECIMAL },
  { "lower-roman", NS_STYLE_LIST_STYLE_LOWER_ROMAN },
  { "upper-roman", NS_STYLE_LIST_STYLE_UPPER_ROMAN },
  { "lower-alpha", NS_STYLE_LIST_STYLE_LOWER_ALPHA },
  { "upper-alpha", NS_STYLE_LIST_STYLE_UPPER_ALPHA },
  { 0 }
};

nsGenericHTMLElement::EnumTable kOtherListTypeTable[] = {
  { "A", NS_STYLE_LIST_STYLE_UPPER_ALPHA },
  { "a", NS_STYLE_LIST_STYLE_LOWER_ALPHA },
  { "I", NS_STYLE_LIST_STYLE_UPPER_ROMAN },
  { "i", NS_STYLE_LIST_STYLE_LOWER_ROMAN },
  { 0 }
};

NS_IMETHODIMP
nsHTMLOListElement::StringToAttribute(nsIAtom* aAttribute,
                                      const nsString& aValue,
                                      nsHTMLValue& aResult)
{
  if (aAttribute == nsHTMLAtoms::type) {
    if (!nsGenericHTMLElement::ParseEnumValue(aValue, kListTypeTable,
                                              aResult)) {
      if (!nsGenericHTMLElement::ParseCaseSensitiveEnumValue(aValue,
                                  kOtherListTypeTable, aResult)) {
        aResult.SetIntValue(NS_STYLE_LIST_STYLE_DECIMAL, eHTMLUnit_Enumerated);
      }
    }
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  if (aAttribute == nsHTMLAtoms::start) {
    nsGenericHTMLElement::ParseValue(aValue, 1, aResult, eHTMLUnit_Integer);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  if (aAttribute == nsHTMLAtoms::compact) {
    aResult.SetEmptyValue();
    return NS_CONTENT_ATTR_NO_VALUE;
  }
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLOListElement::AttributeToString(nsIAtom* aAttribute,
                                      const nsHTMLValue& aValue,
                                      nsString& aResult) const
{
  if (aAttribute == nsHTMLAtoms::type) {
    nsGenericHTMLElement::EnumValueToString(aValue, kListTypeTable, aResult);
    return NS_CONTENT_ATTR_HAS_VALUE;
  }
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(nsIHTMLAttributes* aAttributes,
                  nsIStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  if (nsnull != aAttributes) {
    nsHTMLValue value;
    nsStyleList* list = (nsStyleList*)
      aContext->GetMutableStyleData(eStyleStruct_List);

    // type: enum
    aAttributes->GetAttribute(nsHTMLAtoms::type, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {
      list->mListStyleType = value.GetIntValue();
    }

    // compact: empty
    aAttributes->GetAttribute(nsHTMLAtoms::compact, value);
    if (value.GetUnit() == eHTMLUnit_Empty) {
      // XXX set
    }
  }
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLOListElement::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLOListElement::HandleDOMEvent(nsIPresContext& aPresContext,
                                   nsEvent* aEvent,
                                   nsIDOMEvent** aDOMEvent,
                                   PRUint32 aFlags,
                                   nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

NS_IMETHODIMP
nsHTMLOListElement::GetStyleHintForAttributeChange(
    const nsIAtom* aAttribute,
    PRInt32 *aHint) const
{
  if ((aAttribute == nsHTMLAtoms::start) ||
      (aAttribute == nsHTMLAtoms::compact)) {
    *aHint = NS_STYLE_HINT_CONTENT;
  }
  else if (aAttribute == nsHTMLAtoms::type) {
    *aHint = NS_STYLE_HINT_REFLOW;
  }
  else {
    nsGenericHTMLElement::GetStyleHintForCommonAttributes(this, aAttribute, aHint);
  }

  return NS_OK;
}
