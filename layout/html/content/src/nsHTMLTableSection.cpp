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
#include "nsIDOMHTMLTableSectionElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsHTMLGenericContent.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

static NS_DEFINE_IID(kIDOMHTMLTableSectionElementIID, NS_IDOMHTMLTABLESECTIONELEMENT_IID);

class nsHTMLTableSection : public nsIDOMHTMLTableSectionElement,
                           public nsIScriptObjectOwner,
                           public nsIDOMEventReceiver,
                           public nsIHTMLContent
{
public:
  nsHTMLTableSection(nsIAtom* aTag);
  ~nsHTMLTableSection();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLTableSectionElement
  NS_IMETHOD GetAlign(nsString& aAlign);
  NS_IMETHOD SetAlign(const nsString& aAlign);
  NS_IMETHOD GetVAlign(nsString& aVAlign);
  NS_IMETHOD SetVAlign(const nsString& aVAlign);
  NS_IMETHOD GetRows(nsIDOMHTMLCollection** aRows);
  NS_IMETHOD SetRows(nsIDOMHTMLCollection* aRows);
  NS_IMETHOD InsertRow(PRInt32 aIndex, nsIDOMHTMLElement** aReturn);
  NS_IMETHOD DeleteRow(PRInt32 aIndex);

  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC(mInner)

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC(mInner)

  // nsIHTMLContent
  NS_IMPL_IHTMLCONTENT_USING_GENERIC(mInner)

protected:
  nsHTMLGenericContainerContent mInner;
};

nsresult
NS_NewHTMLTableSection(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLTableSection(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLTableSection::nsHTMLTableSection(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLTableSection::~nsHTMLTableSection()
{
}

NS_IMPL_ADDREF(nsHTMLTableSection)

NS_IMPL_RELEASE(nsHTMLTableSection)

nsresult
nsHTMLTableSection::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLTableSectionElementIID)) {
    nsIDOMHTMLTableSectionElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLTableSection::CloneNode(nsIDOMNode** aReturn)
{
  nsHTMLTableSection* it = new nsHTMLTableSection(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMPL_STRING_ATTR(nsHTMLTableSection, Align, align, eSetAttrNotify_Reflow)
NS_IMPL_STRING_ATTR(nsHTMLTableSection, VAlign, valign, eSetAttrNotify_Reflow)

NS_IMETHODIMP
nsHTMLTableSection::GetRows(nsIDOMHTMLCollection** aValue)
{
  *aValue = 0;
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableSection::SetRows(nsIDOMHTMLCollection* aValue)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableSection::InsertRow(PRInt32 aIndex, nsIDOMHTMLElement** aValue)
{
  *aValue = 0;
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableSection::DeleteRow(PRInt32 aIndex)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableSection::StringToAttribute(nsIAtom* aAttribute,
                                      const nsString& aValue,
                                      nsHTMLValue& aResult)
{
  // XXX write me
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLTableSection::AttributeToString(nsIAtom* aAttribute,
                                      nsHTMLValue& aValue,
                                      nsString& aResult) const
{
  // XXX write me
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLTableSection::MapAttributesInto(nsIStyleContext* aContext,
                                      nsIPresContext* aPresContext)
{
  // XXX write me
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLTableSection::HandleDOMEvent(nsIPresContext& aPresContext,
                                   nsEvent* aEvent,
                                   nsIDOMEvent** aDOMEvent,
                                   PRUint32 aFlags,
                                   nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}
