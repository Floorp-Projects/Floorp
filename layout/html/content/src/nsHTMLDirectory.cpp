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
#include "nsIDOMHTMLDirectoryElement.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIHTMLContent.h"
#include "nsHTMLGenericContent.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"

static NS_DEFINE_IID(kIDOMHTMLDirectoryElementIID, NS_IDOMHTMLDIRECTORYELEMENT_IID);

class nsHTMLDirectory : public nsIDOMHTMLDirectoryElement,
                        public nsIScriptObjectOwner,
                        public nsIDOMEventReceiver,
                        public nsIHTMLContent
{
public:
  nsHTMLDirectory(nsIAtom* aTag);
  ~nsHTMLDirectory();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC(mInner)

  // nsIDOMElement
  NS_IMPL_IDOMELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLElement
  NS_IMPL_IDOMHTMLELEMENT_USING_GENERIC(mInner)

  // nsIDOMHTMLDirectoryElement
  NS_IMETHOD GetCompact(PRBool* aCompact);
  NS_IMETHOD SetCompact(PRBool aCompact);

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
NS_NewHTMLDirectory(nsIHTMLContent** aInstancePtrResult, nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* it = new nsHTMLDirectory(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsHTMLDirectory::nsHTMLDirectory(nsIAtom* aTag)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aTag);
}

nsHTMLDirectory::~nsHTMLDirectory()
{
}

NS_IMPL_ADDREF(nsHTMLDirectory)

NS_IMPL_RELEASE(nsHTMLDirectory)

nsresult
nsHTMLDirectory::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_HTML_CONTENT_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMHTMLDirectoryElementIID)) {
    nsIDOMHTMLDirectoryElement* tmp = this;
    *aInstancePtr = (void*) tmp;
    mRefCnt++;
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult
nsHTMLDirectory::CloneNode(nsIDOMNode** aReturn)
{
  nsHTMLDirectory* it = new nsHTMLDirectory(mInner.mTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsHTMLDirectory::GetCompact(PRBool* aValue)
{
  nsHTMLValue val;
  *aValue = NS_CONTENT_ATTR_HAS_VALUE ==
    mInner.GetAttribute(nsHTMLAtoms::compact, val);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDirectory::SetCompact(PRBool aValue)
{
  nsAutoString empty;
  if (aValue) {
    return mInner.SetAttr(nsHTMLAtoms::compact, empty, eSetAttrNotify_Reflow);
  }
  else {
    mInner.UnsetAttribute(nsHTMLAtoms::compact);
    return NS_OK;
  }
}

NS_IMETHODIMP
nsHTMLDirectory::StringToAttribute(nsIAtom* aAttribute,
                                   const nsString& aValue,
                                   nsHTMLValue& aResult)
{
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLDirectory::AttributeToString(nsIAtom* aAttribute,
                                   nsHTMLValue& aValue,
                                   nsString& aResult) const
{
  return mInner.AttributeToString(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLDirectory::MapAttributesInto(nsIStyleContext* aContext,
                                   nsIPresContext* aPresContext)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLDirectory::HandleDOMEvent(nsIPresContext& aPresContext,
                                nsEvent* aEvent,
                                nsIDOMEvent** aDOMEvent,
                                PRUint32 aFlags,
                                nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}
