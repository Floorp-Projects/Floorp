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
#include "nsIDOMHTMLLinkElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsGenericHTMLElement.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIDOMStyleSheet.h"
#include "nsIStyleSheet.h"
#include "nsIStyleSheetLinkingElement.h"

static NS_DEFINE_IID(kIDOMHTMLLinkElementIID, NS_IDOMHTMLLINKELEMENT_IID);
static NS_DEFINE_IID(kIStyleSheetLinkingElementIID, NS_ISTYLESHEETLINKINGELEMENT_IID);
static NS_DEFINE_IID(kIDOMStyleSheetIID, NS_IDOMSTYLESHEET_IID);

class nsHTMLLinkElement : public nsIDOMHTMLLinkElement,
                          public nsIScriptObjectOwner,
                          public nsIDOMEventReceiver,
                          public nsIHTMLContent,
                          public nsIStyleSheetLinkingElement
{
public:
  nsHTMLLinkElement(nsIAtom* aTag);
  virtual ~nsHTMLLinkElement();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLLinkElement
  NS_IMETHOD GetDisabled(PRBool* aDisabled);
  NS_IMETHOD SetDisabled(PRBool aDisabled);
  NS_IMETHOD GetCharset(nsString& aCharset);
  NS_IMETHOD SetCharset(const nsString& aCharset);
  NS_IMETHOD GetHref(nsString& aHref);
  NS_IMETHOD SetHref(const nsString& aHref);
  NS_IMETHOD GetHreflang(nsString& aHreflang);
  NS_IMETHOD SetHreflang(const nsString& aHreflang);
  NS_IMETHOD GetMedia(nsString& aMedia);
  NS_IMETHOD SetMedia(const nsString& aMedia);
  NS_IMETHOD GetRel(nsString& aRel);
  NS_IMETHOD SetRel(const nsString& aRel);
  NS_IMETHOD GetRev(nsString& aRev);
  NS_IMETHOD SetRev(const nsString& aRev);
  NS_IMETHOD GetTarget(nsString& aTarget);
  NS_IMETHOD SetTarget(const nsString& aTarget);
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

  // nsIStyleSheetLinkingElement  
  NS_IMETHOD SetStyleSheet(nsIStyleSheet* aStyleSheet);
  NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aStyleSheet);

protected:
  nsGenericHTMLLeafElement mInner;
  nsIStyleSheet* mStyleSheet;
};

nsresult
NS_NewHTMLLinkElement(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLLinkElement(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLLinkElement::nsHTMLLinkElement(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
  mStyleSheet = nsnull;
}

nsHTMLLinkElement::~nsHTMLLinkElement()
{
  NS_IF_RELEASE(mStyleSheet);
}

NS_IMPL_ADDREF(nsHTMLLinkElement)

NS_IMPL_RELEASE(nsHTMLLinkElement)

nsresult
nsHTMLLinkElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLLinkElementIID)) {
    nsIDOMHTMLLinkElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIStyleSheetLinkingElementIID)) {
    nsIStyleSheetLinkingElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLLinkElement::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsHTMLLinkElement* it = new nsHTMLLinkElement(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner, aDeep);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLLinkElement::GetDisabled(PRBool* aDisabled)
{
  nsresult result = NS_OK;
  
  if (nsnull != mStyleSheet) {
    nsIDOMStyleSheet* ss;
    
    result = mStyleSheet->QueryInterface(kIDOMStyleSheetIID, (void**)&ss);
    if (NS_OK == result) {
      result = ss->GetDisabled(aDisabled);
      NS_RELEASE(ss);
    }
  }
  else {
    *aDisabled = PR_FALSE;
  }
  
  return result;
}

NS_IMETHODIMP 
nsHTMLLinkElement::SetDisabled(PRBool aDisabled)
{
  nsresult result = NS_OK;
  
  if (nsnull != mStyleSheet) {
    nsIDOMStyleSheet* ss;
    
    result = mStyleSheet->QueryInterface(kIDOMStyleSheetIID, (void**)&ss);
    if (NS_OK == result) {
      result = ss->SetDisabled(aDisabled);
      NS_RELEASE(ss);
    }
  }
  
  return result;
}

NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Charset, charset)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Href, href)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Hreflang, hreflang)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Media, media)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Rel, rel)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Rev, rev)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Target, target)
NS_IMPL_STRING_ATTR(nsHTMLLinkElement, Type, type)

NS_IMETHODIMP 
nsHTMLLinkElement::SetStyleSheet(nsIStyleSheet* aStyleSheet)
{
  NS_IF_RELEASE(mStyleSheet);

  mStyleSheet = aStyleSheet;
  NS_IF_ADDREF(mStyleSheet);

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLLinkElement::GetStyleSheet(nsIStyleSheet*& aStyleSheet)
{
  aStyleSheet = mStyleSheet;
  NS_IF_ADDREF(aStyleSheet);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLinkElement::StringToAttribute(nsIAtom* aAttribute,
                              const nsString& aValue,
                              nsHTMLValue& aResult)
{
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLLinkElement::AttributeToString(nsIAtom* aAttribute,
                              const nsHTMLValue& aValue,
                              nsString& aResult) const
{
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

static void
MapAttributesInto(nsIHTMLAttributes* aAttributes,
                  nsIStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsHTMLLinkElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                                nsMapAttributesFunc& aMapFunc) const
{
  aFontMapFunc = nsnull;
  aMapFunc = &MapAttributesInto;
  return NS_OK;
}


NS_IMETHODIMP
nsHTMLLinkElement::HandleDOMEvent(nsIPresContext& aPresContext,
                           nsEvent* aEvent,
                           nsIDOMEvent** aDOMEvent,
                           PRUint32 aFlags,
                           nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

NS_IMETHODIMP
nsHTMLLinkElement::GetStyleHintForAttributeChange(
    const nsIAtom* aAttribute,
    PRInt32 *aHint) const
{
  nsGenericHTMLElement::GetStyleHintForCommonAttributes(this, aAttribute, aHint);
  return NS_OK;
}

