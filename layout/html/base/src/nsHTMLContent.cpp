/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsHTMLContent.h"
#include "nsString.h"
#include "nsIArena.h"
#include "nsIAtom.h"
#include "nsIHTMLAttributes.h"
#include "nsIContentDelegate.h"
#include "nsFrame.h"
#include "nsHTMLIIDs.h"
#include "nsISupportsArray.h"
#include "nsCRT.h"
#include "nsIDocument.h"
#include "nsEventListenerManager.h"
#include "nsIEventStateManager.h"
#include "nsISizeOfHandler.h"
#include "nsDOMEvent.h"
#include "nsIPrivateDOMEvent.h"

static NS_DEFINE_IID(kIContentDelegateIID, NS_ICONTENTDELEGATE_IID);
static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIEventListenerManagerIID, NS_IEVENTLISTENERMANAGER_IID);
static NS_DEFINE_IID(kIDOMEventReceiverIID, NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIPrivateDOMEventIID, NS_IPRIVATEDOMEVENT_IID);

//----------------------------------------------------------------------

void* nsHTMLContent::operator new(size_t size)
{
  nsHTMLContent* rv = (nsHTMLContent*) ::operator new(size);
  nsCRT::zero(rv, size);
  rv->mInHeap = 1;
  return (void*) rv;
}

void* nsHTMLContent::operator new(size_t size, nsIArena* aArena)
{
  nsHTMLContent* rv = (nsHTMLContent*) aArena->Alloc(PRInt32(size));
  nsCRT::zero(rv, size);
  return (void*) rv;
}

void nsHTMLContent::operator delete(void* ptr)
{
  NS_PRECONDITION(nsnull != ptr, "null ptr");
  nsHTMLContent* hc = (nsHTMLContent*) ptr;
  if (nsnull != hc) {
    if (hc->mInHeap) {
      ::delete ptr;
    }
  }
}

nsHTMLContent::nsHTMLContent()
{
  mListenerManager = nsnull;
}

nsHTMLContent::~nsHTMLContent()
{
  NS_IF_RELEASE(mListenerManager);
}

NS_IMPL_ADDREF(nsHTMLContent)
NS_IMPL_RELEASE(nsHTMLContent)

nsresult nsHTMLContent::QueryInterface(const nsIID& aIID,
                                       void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIHTMLContentIID)) {
    *aInstancePtrResult = (void*) ((nsIHTMLContent*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIContentIID)) {
    *aInstancePtrResult = (void*) ((nsIContent*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIContent*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMNodeIID)) {
    *aInstancePtrResult = (void*)(nsIDOMNode*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    *aInstancePtrResult = (void*)(nsIScriptObjectOwner*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMEventReceiverIID)) {
    *aInstancePtrResult = (void*)(nsIDOMEventReceiver*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_METHOD
nsHTMLContent::IsSynthetic(PRBool& aResult)
{
  aResult = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::Compact()
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::GetDocument(nsIDocument*& aResult) const
{
  aResult = mDocument;
  NS_IF_ADDREF(mDocument);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::SetDocument(nsIDocument* aDocument)
{
  mDocument = aDocument;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::GetParent(nsIContent*& aResult) const
{
  NS_IF_ADDREF(mParent);
  aResult = mParent;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::SetParent(nsIContent* aParent)
{
  mParent = aParent;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::CanContainChildren(PRBool& aResult) const
{
  aResult = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::ChildCount(PRInt32& aCount) const
{
  aCount = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::ChildAt(PRInt32 aIndex, nsIContent*& aResult) const
{
  aResult = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const
{
  aResult = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::AppendChildTo(nsIContent* aKid, PRBool aNotify)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::RemoveChildAt(PRInt32 aIndex, PRBool aNotify)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::SetAttribute(const nsString& aName, const nsString& aValue,
                            PRBool aNotify)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::GetAttribute(const nsString& aName,
                            nsString& aResult) const
{
  aResult.SetLength(0);
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLContent::SetAttribute(nsIAtom* aAttribute, const nsString& aValue,
                            PRBool aNotify)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::GetAttribute(nsIAtom *aAttribute,
                            nsString &aResult) const
{
  aResult.SetLength(0);
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLContent::SetAttribute(nsIAtom* aAttribute, const nsHTMLValue& aValue,
                            PRBool aNotify)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::UnsetAttribute(nsIAtom* aAttribute)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::GetAttribute(nsIAtom* aAttribute,
                            nsHTMLValue& aValue) const
{
  aValue.Reset();
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsHTMLContent::GetAllAttributeNames(nsISupportsArray* aArray,
                                    PRInt32& aCount) const
{
  aCount = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::GetAttributeCount(PRInt32& aCount) const 
{
  aCount = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::SetID(nsIAtom* aID)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::GetID(nsIAtom*& aResult) const
{
  aResult = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::SetClass(nsIAtom* aClass)
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::GetClass(nsIAtom*& aResult) const
{
  aResult = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::GetStyleRule(nsIStyleRule*& aResult)
{
  aResult = nsnull;
  return NS_OK;
}

void nsHTMLContent::ListAttributes(FILE* out) const
{
  nsISupportsArray* attrs;
  if (NS_OK == NS_NewISupportsArray(&attrs)) {
    PRInt32 index, count;
    GetAllAttributeNames(attrs, count);
    for (index = 0; index < count; index++) {
      // name
      nsIAtom* attr = (nsIAtom*)attrs->ElementAt(index);
      nsAutoString buffer;
      attr->ToString(buffer);

      // value
      nsAutoString value;
      GetAttribute(buffer, value);
      buffer.Append("=");
      buffer.Append(value);

      fputs(" ", out);
      fputs(buffer, out);
      NS_RELEASE(attr);
    }
    NS_RELEASE(attrs);
  }
}

NS_IMETHODIMP
nsHTMLContent::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(nsnull != mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  nsIAtom* tag;
  GetTag(tag);
  if (tag != nsnull) {
    nsAutoString buf;
    tag->ToString(buf);
    fputs(buf, out);
    NS_RELEASE(tag);
  }

  ListAttributes(out);

  fprintf(out, " RefCount=%d<\n", mRefCnt);

  PRBool canHaveKids;
  CanContainChildren(canHaveKids);
  if (canHaveKids) {
    PRInt32 kids;
    ChildCount(kids);
    for (index = 0; index < kids; index++) {
      nsIContent* kid;
      ChildAt(index, kid);
      kid->List(out, aIndent + 1);
      NS_RELEASE(kid);
    }
  }

  for (index = aIndent; --index >= 0; ) fputs("  ", out);
  fputs(">\n", out);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::SizeOf(nsISizeOfHandler* aHandler) const
{
  aHandler->Add(sizeof(*this));
  nsHTMLContent::SizeOfWithoutThis(aHandler);
  return NS_OK;
}

void
nsHTMLContent::SizeOfWithoutThis(nsISizeOfHandler* aHandler) const
{
  // XXX mScriptObject
}

NS_IMETHODIMP
nsHTMLContent::GetTag(nsIAtom*& aResult) const
{
  aResult = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContent::ToHTML(FILE* out) const
{
  nsAutoString tmp;
  ToHTMLString(tmp);
  fputs(tmp, out);
  return NS_OK;
}

nsresult nsHTMLContent::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

//
// Implementation of nsIDOMNode interface
//
NS_IMETHODIMP    
nsHTMLContent::GetParentNode(nsIDOMNode** aParentNode)
{
  if (nsnull != mParent) {
    nsresult res = mParent->QueryInterface(kIDOMNodeIID, (void**)aParentNode);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Node");
    return res;
  }
  else {
    *aParentNode = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLContent::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  *aChildNodes = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLContent::GetHasChildNodes(PRBool* aHasChildNodes)
{
  *aHasChildNodes = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLContent::GetFirstChild(nsIDOMNode** aFirstChild)
{
  *aFirstChild = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLContent::GetLastChild(nsIDOMNode** aLastChild)
{
  *aLastChild = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLContent::GetPreviousSibling(nsIDOMNode** aNode)
{
  if (nsnull != mParent) {
    PRInt32 pos;
    mParent->IndexOf(this, pos);
    if (pos > -1) {
      nsIContent* prev;
      mParent->ChildAt(--pos, prev);
      if (nsnull != prev) {
        nsresult res = prev->QueryInterface(kIDOMNodeIID, (void**)aNode);
        NS_ASSERTION(NS_OK == res, "Must be a DOM Node");
        
        NS_RELEASE(prev); // balance the AddRef in ChildAt()
        
        return res;
      }
    }
  }

  *aNode = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLContent::GetNextSibling(nsIDOMNode** aNextSibling)
{
  if (nsnull != mParent) {
    PRInt32 pos;
    mParent->IndexOf(this, pos);
    if (pos > -1 ) {
      nsIContent* prev;
      mParent->ChildAt(++pos, prev);
      if (nsnull != prev) {
        nsresult res = prev->QueryInterface(kIDOMNodeIID, (void**)aNextSibling);
        NS_ASSERTION(NS_OK == res, "Must be a DOM Node");
        NS_RELEASE(prev); // balance the AddRef in ChildAt()

        return res;
      }
    }
  }

  *aNextSibling = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLContent::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
  *aAttributes = nsnull;
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLContent::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP    
nsHTMLContent::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP    
nsHTMLContent::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP    
nsHTMLContent::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_FAILURE;
}

nsresult nsHTMLContent::GetListenerManager(nsIEventListenerManager **aInstancePtrResult)
{
  if (nsnull != mListenerManager) {
    return mListenerManager->QueryInterface(kIEventListenerManagerIID, (void**) aInstancePtrResult);;
  }
  if (NS_OK == NS_NewEventListenerManager(aInstancePtrResult)) {
    mListenerManager = *aInstancePtrResult;
    NS_ADDREF(mListenerManager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult nsHTMLContent::GetNewListenerManager(nsIEventListenerManager **aInstancePtrResult)
{
  return NS_NewEventListenerManager(aInstancePtrResult);
} 

nsresult nsHTMLContent::AddEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  nsIEventListenerManager *mManager;

  if (NS_OK == GetListenerManager(&mManager)) {
    mManager->AddEventListener(aListener, aIID);
    NS_RELEASE(mManager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult nsHTMLContent::RemoveEventListener(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  if (nsnull != mListenerManager) {
    mListenerManager->RemoveEventListener(aListener, aIID);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult nsHTMLContent::HandleDOMEvent(nsIPresContext& aPresContext,
                                       nsEvent* aEvent,
                                       nsIDOMEvent** aDOMEvent,
                                       PRUint32 aFlags,
                                       nsEventStatus& aEventStatus)
{  
  nsresult ret = NS_OK;
  nsIDOMEvent* DOMEvent = nsnull;

  if (DOM_EVENT_INIT == aFlags) {
    nsIEventStateManager *mManager;
    if (NS_OK == aPresContext.GetEventStateManager(&mManager)) {
      mManager->SetEventTarget((nsIHTMLContent*)this);
      NS_RELEASE(mManager);
    }
 
    aDOMEvent = &DOMEvent;
  }
  
  //Capturing stage
  /*if (mDocument->GetEventCapturer) {
    ret = mEventCapturer->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, aFlags, aEventStatus);
  }*/
  
  //Local handling stage
  if (nsnull != mListenerManager) {
    ret = mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, aEventStatus);
  }

  //Bubbling stage
  if (DOM_EVENT_CAPTURE != aFlags && mParent != nsnull) {
    ret = mParent->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, DOM_EVENT_BUBBLE, aEventStatus);
  }

  if (DOM_EVENT_INIT == aFlags) {
    // We're leaving the DOM event loop so if we created a DOM event, release here.
    if (nsnull != *aDOMEvent) {
      nsrefcnt rc;
      NS_RELEASE2(*aDOMEvent, rc);
      if (0 != rc) {
      //Okay, so someone in the DOM loop (a listener, JS object) still has a ref to the DOM Event but
      //the internal data hasn't been malloc'd.  Force a copy of the data here so the DOM Event is still valid.
        nsIPrivateDOMEvent *privateEvent;
        if (NS_OK == (*aDOMEvent)->QueryInterface(kIPrivateDOMEventIID, (void**)&privateEvent)) {
          privateEvent->DuplicatePrivateData();
          NS_RELEASE(privateEvent);
        }
      }
    }
    aDOMEvent = nsnull;
  }

  return ret;
}

// XXX i18n: this is wrong (?) because we need to know the outgoing
// character set (I think)
void
nsHTMLContent::QuoteForHTML(const nsString& aValue, nsString& aResult)
{
  aResult.Truncate();
  const PRUnichar* cp = aValue.GetUnicode();
  const PRUnichar* end = aValue.GetUnicode() + aValue.Length();
  aResult.Append('"');
  while (cp < end) {
    PRUnichar ch = *cp++;
    if ((ch >= 0x20) && (ch <= 0x7f)) {
      if (ch == '\"') {
        aResult.Append("&quot;");
      }
      else {
        aResult.Append(ch);
      }
    }
    else {
      aResult.Append("&#");
      aResult.Append((PRInt32) ch, 10);
      aResult.Append(';');
    }
  }
  aResult.Append('"');
}
