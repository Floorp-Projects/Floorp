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

#include "nsISupports.h"
#include "nsIContent.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIScriptObjectOwner.h"
#include "nsGenericElement.h"
#include "nsINameSpaceManager.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMScriptObjectFactory.h"


static NS_DEFINE_IID(kIDOMDocumentFragmentIID, NS_IDOMDOCUMENTFRAGMENT_IID);
static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);

class nsDocumentFragment : public nsIContent,
                           public nsIDOMDocumentFragment,
                           public nsIScriptObjectOwner
{
public:
  nsDocumentFragment(nsIDocument* aOwnerDocument);
  virtual ~nsDocumentFragment();

  // nsISupports
  NS_DECL_ISUPPORTS

  // interface nsIDOMDocumentFragment
  NS_IMETHOD    GetNodeName(nsString& aNodeName);
  NS_IMETHOD    GetNodeValue(nsString& aNodeValue);
  NS_IMETHOD    SetNodeValue(const nsString& aNodeValue);
  NS_IMETHOD    GetNodeType(PRUint16* aNodeType);
  NS_IMETHOD    GetParentNode(nsIDOMNode** aParentNode)
    { 
      *aParentNode = nsnull;
      return NS_OK;
    }
  NS_IMETHOD    GetChildNodes(nsIDOMNodeList** aChildNodes)
    { return mInner.GetChildNodes(aChildNodes); }
  NS_IMETHOD    GetFirstChild(nsIDOMNode** aFirstChild)
    { return mInner.GetFirstChild(aFirstChild); }
  NS_IMETHOD    GetLastChild(nsIDOMNode** aLastChild)
    { return mInner.GetLastChild(aLastChild); }
  NS_IMETHOD    GetPreviousSibling(nsIDOMNode** aPreviousSibling)
    {
      *aPreviousSibling = nsnull;
      return NS_OK;
    }
  NS_IMETHOD    GetNextSibling(nsIDOMNode** aNextSibling)
    {
      *aNextSibling = nsnull;
      return NS_OK;
    }
  NS_IMETHOD    GetAttributes(nsIDOMNamedNodeMap** aAttributes)
    {
      *aAttributes = nsnull;
      return NS_OK;
    }
  NS_IMETHOD    GetOwnerDocument(nsIDOMDocument** aOwnerDocument);
  NS_IMETHOD    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, 
                             nsIDOMNode** aReturn)
    { return mInner.InsertBefore(aNewChild, aRefChild, aReturn); }
  NS_IMETHOD    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, 
                             nsIDOMNode** aReturn)
    { return mInner.ReplaceChild(aNewChild, aOldChild, aReturn); }
  NS_IMETHOD    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
    { return mInner.RemoveChild(aOldChild, aReturn); }
  NS_IMETHOD    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
    { return mInner.AppendChild(aNewChild, aReturn); }
  NS_IMETHOD    HasChildNodes(PRBool* aReturn)
    { return mInner.HasChildNodes(aReturn); }
  NS_IMETHOD    CloneNode(PRBool aDeep, nsIDOMNode** aReturn);

  // interface nsIScriptObjectOwner
  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void* aScriptObject);

  // interface nsIContent
  NS_IMETHOD ParseAttributeString(const nsString& aStr, 
                                  nsIAtom*& aName,
                                  PRInt32& aNameSpaceID)
    { aName = nsnull;
      aNameSpaceID = kNameSpaceID_None;
      return NS_OK; 
    }
  NS_IMETHOD GetNameSpacePrefixFromId(PRInt32 aNameSpaceID,
                                      nsIAtom*& aPrefix)
    {
      aPrefix = nsnull;
      return NS_OK;
    }
  NS_IMETHOD GetDocument(nsIDocument*& aResult) const
    { return mInner.GetDocument(aResult); }
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep)
    { return mInner.SetDocument(aDocument, aDeep); }
  NS_IMETHOD GetParent(nsIContent*& aResult) const
    {
      aResult = nsnull;
      return NS_OK;
    }
  NS_IMETHOD SetParent(nsIContent* aParent)
    { return NS_OK; }
  NS_IMETHOD GetNameSpaceID(PRInt32& aResult) const
    {
      aResult = kNameSpaceID_None;
      return NS_OK;
    }
  NS_IMETHOD GetTag(nsIAtom*& aResult) const
    {
      aResult = nsnull;
      return NS_OK;
    }
  NS_IMETHOD CanContainChildren(PRBool& aResult) const
    { return mInner.CanContainChildren(aResult); }
  NS_IMETHOD ChildCount(PRInt32& aResult) const
    { return mInner.ChildCount(aResult); }
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const
    { return mInner.ChildAt(aIndex, aResult); }
  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aIndex) const
    { return mInner.IndexOf(aPossibleChild, aIndex); }
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex,
                           PRBool aNotify)
    { return mInner.InsertChildAt(aKid, aIndex, aNotify); }
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,
                            PRBool aNotify)
    { return mInner.ReplaceChildAt(aKid, aIndex, aNotify); }
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify)
    { return mInner.AppendChildTo(aKid, aNotify); }  
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify)
    { return mInner.RemoveChildAt(aIndex, aNotify); }
  NS_IMETHOD IsSynthetic(PRBool& aResult)
    { return mInner.IsSynthetic(aResult); }
  NS_IMETHOD SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                          const nsString& aValue,
                          PRBool aNotify)
    { return NS_OK; }
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, 
                          nsString& aResult) const
    { return NS_CONTENT_ATTR_NOT_THERE; }
  NS_IMETHOD UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute, 
                            PRBool aNotify)
    { return NS_OK; }
  NS_IMETHOD GetAttributeNameAt(PRInt32 aIndex,
                                PRInt32& aNameSpaceID, 
                                nsIAtom*& aName) const
    { 
      aName = nsnull;
      return NS_ERROR_ILLEGAL_VALUE;
    }
  NS_IMETHOD GetAttributeCount(PRInt32& aCountResult) const
    { 
      aCountResult = 0;
      return NS_OK;
    }
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const
    { return mInner.List(out, aIndent); }
  NS_IMETHOD BeginConvertToXIF(nsXIFConverter& aConverter) const
    { return NS_OK; }
  NS_IMETHOD ConvertContentToXIF(nsXIFConverter& aConverter) const
    { return NS_OK; }
  NS_IMETHOD FinishConvertToXIF(nsXIFConverter& aConverter) const
    { return NS_OK; }
  NS_IMETHOD HandleDOMEvent(nsIPresContext& aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus& aEventStatus)
    {
      aEventStatus = nsEventStatus_eIgnore;
      return NS_OK;
    }
  NS_IMETHOD RangeAdd(nsIDOMRange& aRange)
    {  return mInner.RangeAdd(aRange); } 
  NS_IMETHOD RangeRemove(nsIDOMRange& aRange)
    {  return mInner.RangeRemove(aRange); }                                                                        
  NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const 
    {  return mInner.GetRangeList(aResult); }                                                                        

protected:
  nsGenericContainerElement mInner;
  void* mScriptObject;
  nsIDocument* mOwnerDocument;
};

nsresult
NS_NewDocumentFragment(nsIDOMDocumentFragment** aInstancePtrResult,
                       nsIDocument* aOwnerDocument)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsDocumentFragment* it = new nsDocumentFragment(aOwnerDocument);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIDOMDocumentFragmentIID, 
                            (void**) aInstancePtrResult);
}

nsDocumentFragment::nsDocumentFragment(nsIDocument* aOwnerDocument)
{
  NS_INIT_REFCNT();
  mInner.Init(this, nsnull);
  mScriptObject = nsnull;
  mOwnerDocument = aOwnerDocument;
  NS_IF_ADDREF(mOwnerDocument);
}

nsDocumentFragment::~nsDocumentFragment()
{
  NS_IF_RELEASE(mOwnerDocument);
}
 
NS_IMPL_ADDREF(nsDocumentFragment)
NS_IMPL_RELEASE(nsDocumentFragment)

nsresult
nsDocumentFragment::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (aIID.Equals(kISupportsIID)) {
    nsIDOMDocumentFragment* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMDocumentFragmentIID)) {
    nsIDOMDocumentFragment* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMNodeIID)) {
    nsIDOMDocumentFragment* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    nsIScriptObjectOwner* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIContentIID)) {
    nsIContent* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP    
nsDocumentFragment::GetNodeName(nsString& aNodeName)
{
  aNodeName.SetString("#document-fragment");
  return NS_OK;
}

NS_IMETHODIMP    
nsDocumentFragment::GetNodeValue(nsString& aNodeValue)
{
  aNodeValue.Truncate();
  return NS_OK;
}

NS_IMETHODIMP    
nsDocumentFragment::SetNodeValue(const nsString& aNodeValue)
{
  return NS_OK;
}

NS_IMETHODIMP    
nsDocumentFragment::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::DOCUMENT_FRAGMENT_NODE;
  return NS_OK;
}

NS_IMETHODIMP    
nsDocumentFragment::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  if (nsnull != mOwnerDocument) {
    return mOwnerDocument->QueryInterface(kIDOMDocumentIID, (void **)aOwnerDocument);
  }
  else {
    *aOwnerDocument = nsnull;
    return NS_OK;
  }
}


NS_IMETHODIMP    
nsDocumentFragment::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsDocumentFragment* it;
  it = new nsDocumentFragment(mOwnerDocument);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
//XXX  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP 
nsDocumentFragment::GetScriptObject(nsIScriptContext* aContext, 
                                    void** aScriptObject)
{
  nsresult res = NS_OK;

  if (nsnull == mScriptObject) {
    nsIDOMScriptObjectFactory *factory;
    
    res = mInner.GetScriptObjectFactory(&factory);
    if (NS_OK != res) {
      return res;
    }
    
    res = factory->NewScriptDocumentFragment(aContext, 
                                             (nsISupports*)(nsIDOMDocumentFragment*)this, 
                                             mOwnerDocument,
                                             (void**)&mScriptObject);
    NS_RELEASE(factory);
    
  }
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP 
nsDocumentFragment::SetScriptObject(void* aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

