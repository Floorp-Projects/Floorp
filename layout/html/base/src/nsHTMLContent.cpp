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

static NS_DEFINE_IID(kIContentDelegateIID, NS_ICONTENTDELEGATE_IID);
static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);

static nsIContentDelegate* gContentDelegate;

/**
 * THE html content delegate. There is exactly one instance of this
 * class and it's used for all html content. It just turns around
 * and asks the content object to create the frame.
 */
class ContentDelegate : public nsIContentDelegate {
public:
  ContentDelegate();
  NS_DECL_ISUPPORTS
  NS_IMETHOD CreateFrame(nsIPresContext* aPresContext,
                         nsIContent* aContent,
                         nsIFrame* aParentFrame,
                         nsIStyleContext* aStyleContext,
                         nsIFrame*& aResult);
protected:
  ~ContentDelegate();
};

ContentDelegate::ContentDelegate()
{
  NS_INIT_REFCNT();
}

NS_IMPL_ISUPPORTS(ContentDelegate, kIContentDelegateIID);

ContentDelegate::~ContentDelegate()
{
}

NS_METHOD
ContentDelegate::CreateFrame(nsIPresContext* aPresContext,
                             nsIContent* aContent,
                             nsIFrame* aParentFrame,
                             nsIStyleContext* aStyleContext,
                             nsIFrame*& aResult)
{
  NS_PRECONDITION(nsnull != aContent, "null ptr");

  // Make sure the content is html content
  nsIHTMLContent* hc;
  nsIFrame* frame = nsnull;
  nsresult rv = aContent->QueryInterface(kIHTMLContentIID, (void**) &hc);
  if (NS_OK != rv) {
    // This means that *somehow* somebody which is not an html
    // content object got ahold of this delegate and tried to
    // create a frame with it. Give them back an nsFrame.
    rv = nsFrame::NewFrame(&frame, aContent, aParentFrame);
    if (NS_OK == rv) {
      frame->SetStyleContext(aPresContext, aStyleContext);
    }
  }
  else {
    // Ask the content object to create the frame
    rv = hc->CreateFrame(aPresContext, aParentFrame, aStyleContext, frame);
    NS_RELEASE(hc);
  }
  aResult = frame;
  return rv;
}

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
  // Create shared content delegate if this is the first html content
  // object being created.
  if (nsnull == gContentDelegate) {
    gContentDelegate = new ContentDelegate();
  }

  // Add a reference to the shared content delegate object
  NS_ADDREF(gContentDelegate);
}

nsHTMLContent::~nsHTMLContent()
{
  NS_PRECONDITION(nsnull != gContentDelegate, "null content delegate");
  if (nsnull != gContentDelegate) {
    // Remove our reference to the shared content delegate object.  If
    // the last reference just went away, null out gContentDelegate.
    nsrefcnt rc = gContentDelegate->Release();
    if (0 == rc) {
      gContentDelegate = nsnull;
    }
  }
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
  return NS_NOINTERFACE;
}

NS_METHOD
nsHTMLContent::IsSynthetic(PRBool& aResult)
{
  aResult = PR_FALSE;
  return NS_OK;
}

void nsHTMLContent::Compact()
{
}

nsIDocument* nsHTMLContent::GetDocument() const
{
  NS_IF_ADDREF(mDocument);
  return mDocument;
}

void nsHTMLContent::SetDocument(nsIDocument* aDocument)
{
  mDocument = aDocument;
}

nsIContent* nsHTMLContent::GetParent() const
{
  NS_IF_ADDREF(mParent);
  return mParent;
}

void nsHTMLContent::SetParent(nsIContent* aParent)
{
  mParent = aParent;
}

PRBool nsHTMLContent::CanContainChildren() const
{
  return PR_FALSE;
}

PRInt32 nsHTMLContent::ChildCount() const
{
  return 0;
}

nsIContent* nsHTMLContent::ChildAt(PRInt32 aIndex) const
{
  return nsnull;
}

PRInt32 nsHTMLContent::IndexOf(nsIContent* aPossibleChild) const
{
  return -1;
}

PRBool nsHTMLContent::InsertChildAt(nsIContent* aKid, PRInt32 aIndex)
{
  return PR_FALSE;
}

PRBool nsHTMLContent::ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex)
{
  return PR_FALSE;
}

PRBool nsHTMLContent::AppendChild(nsIContent* aKid)
{
  return PR_FALSE;
}

PRBool nsHTMLContent::RemoveChildAt(PRInt32 aIndex)
{
  return PR_FALSE;
}

void nsHTMLContent::SetAttribute(const nsString& aName, const nsString& aValue)
{
}

nsContentAttr nsHTMLContent::GetAttribute(const nsString& aName,
                                          nsString& aResult) const
{
  aResult.SetLength(0);
  return eContentAttr_NotThere;
}

void nsHTMLContent::SetAttribute(nsIAtom* aAttribute, const nsString& aValue)
{
}

void nsHTMLContent::SetAttribute(nsIAtom* aAttribute, const nsHTMLValue& aValue)
{
}

void nsHTMLContent::UnsetAttribute(nsIAtom* aAttribute)
{
}

nsContentAttr nsHTMLContent::GetAttribute(nsIAtom* aAttribute,
                                          nsHTMLValue& aValue) const
{
  aValue.Reset();
  return eContentAttr_NotThere;
}

PRInt32 nsHTMLContent::GetAllAttributeNames(nsISupportsArray* aArray) const
{
  return 0;
}

PRInt32 nsHTMLContent::GetAttributeCount(void) const 
{
  return 0;
}

void nsHTMLContent::SetID(nsIAtom* aID)
{
}

nsIAtom* nsHTMLContent::GetID(void) const
{
  return nsnull;
}

void nsHTMLContent::SetClass(nsIAtom* aClass)
{
}

nsIAtom* nsHTMLContent::GetClass(void) const
{
  return nsnull;
}

nsIStyleRule* nsHTMLContent::GetStyleRule(void)
{
  return nsnull;
}

void nsHTMLContent::MapAttributesInto(nsIStyleContext* aContext, 
                                      nsIPresContext* aPresContext)
{
}


void nsHTMLContent::ListAttributes(FILE* out) const
{
  nsISupportsArray* attrs;
  if (NS_OK == NS_NewISupportsArray(&attrs)) {
    GetAllAttributeNames(attrs);
    PRInt32 count = attrs->Count();
    PRInt32 index;
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

void nsHTMLContent::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(nsnull != mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  nsIAtom* tag = GetTag();
  if (tag != nsnull) {
    nsAutoString buf;
    tag->ToString(buf);
    fputs(buf, out);
    NS_RELEASE(tag);
  }

  ListAttributes(out);

  fprintf(out, " RefCount=%d<\n", mRefCnt);

  if (CanContainChildren()) {
    PRInt32 kids = ChildCount();
    for (index = 0; index < kids; index++) {
      nsIContent* kid = ChildAt(index);
      kid->List(out, aIndent + 1);
      NS_RELEASE(kid);
    }
  }

  for (index = aIndent; --index >= 0; ) fputs("  ", out);
  fputs(">\n", out);
}

PRUint32 nsHTMLContent::SizeOf(nsISizeofHandler* aHandler) const
{
  return 0;
}

nsIAtom* nsHTMLContent::GetTag() const
{
  return nsnull;
}

void nsHTMLContent::ToHTML(FILE* out) const
{
  nsAutoString tmp;
  ToHTMLString(tmp);
  fputs(tmp, out);
}

nsIContentDelegate* nsHTMLContent::GetDelegate(nsIPresContext* aCX)
{
  gContentDelegate->AddRef();
  return gContentDelegate;
}

void nsHTMLContent::NewGlobalAtom(const char* aString, nsIAtom** aAtomResult)
{
  if (nsnull == *aAtomResult) {
    *aAtomResult = NS_NewAtom(aString);
  } else {
    NS_ADDREF(*aAtomResult);
  }
}

void nsHTMLContent::ReleaseGlobalAtom(nsIAtom** aAtomResult)
{
  nsIAtom* atom = *aAtomResult;
  if (nsnull != atom) {
    if (0 == atom->Release()) {
      *aAtomResult = nsnull;
    }
  }
}

nsresult nsHTMLContent::ResetScriptObject()
{
  mScriptObject = nsnull;
  return NS_OK;
}

// nsIDOMNode interface
nsresult nsHTMLContent::GetParentNode(nsIDOMNode **aNode)
{
  if (nsnull != mParent) {
    nsresult res = mParent->QueryInterface(kIDOMNodeIID, (void**)aNode);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Node");
    return res;
  }

  return NS_ERROR_FAILURE;
}

nsresult nsHTMLContent::GetChildNodes(nsIDOMNodeIterator **aIterator)
{
  return NS_ERROR_FAILURE;
}

nsresult nsHTMLContent::HasChildNodes()
{
  return NS_ERROR_FAILURE;
}

nsresult nsHTMLContent::GetFirstChild(nsIDOMNode **aNode)
{
  return NS_ERROR_FAILURE;
}

nsresult nsHTMLContent::GetPreviousSibling(nsIDOMNode **aNode)
{
  if (nsnull != mParent) {
    PRInt32 pos = mParent->IndexOf(this);
    if (pos > -1) {
      nsIContent* prev = mParent->ChildAt(--pos);
      if (nsnull != prev) {
        nsresult res = prev->QueryInterface(kIDOMNodeIID, (void**)aNode);
        NS_ASSERTION(NS_OK == res, "Must be a DOM Node");
        
        NS_RELEASE(prev); // balance the AddRef in ChildAt()
        
        return res;
      }
    }
  }

  return NS_ERROR_FAILURE;
}

nsresult nsHTMLContent::GetNextSibling(nsIDOMNode **aNode)
{
  if (nsnull != mParent) {
    PRInt32 pos = mParent->IndexOf(this);
    if (pos > -1 ) {
      nsIContent* prev = mParent->ChildAt(++pos);
      if (nsnull != prev) {
        nsresult res = prev->QueryInterface(kIDOMNodeIID, (void**)aNode);
        NS_ASSERTION(NS_OK == res, "Must be a DOM Node");
        NS_RELEASE(prev); // balance the AddRef in ChildAt()

        return res;
      }
    }
  }

  return NS_ERROR_FAILURE;
}

nsresult nsHTMLContent::InsertBefore(nsIDOMNode *newChild, nsIDOMNode *refChild)
{
  return NS_ERROR_FAILURE;
}

nsresult nsHTMLContent::ReplaceChild(nsIDOMNode *newChild, 
                                      nsIDOMNode *oldChild)
{
  return NS_ERROR_FAILURE;
}

nsresult nsHTMLContent::RemoveChild(nsIDOMNode *oldChild)
{
  return NS_ERROR_FAILURE;
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
