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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsISupports.h"
#include "nsIContent.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIScriptObjectOwner.h"
#include "nsGenericElement.h"
#include "nsINameSpaceManager.h"
#include "nsINodeInfo.h"
#include "nsNodeInfoManager.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsDOMError.h"


class nsDocumentFragment : public nsIContent,
                           public nsIDOMDocumentFragment,
                           public nsIScriptObjectOwner
{
public:
  nsDocumentFragment(nsIDocument* aOwnerDocument, nsINodeInfo *aNodeInfo);
  virtual ~nsDocumentFragment();

  // nsISupports
  NS_DECL_ISUPPORTS

  // interface nsIDOMDocumentFragment
  NS_IMETHOD    GetNodeName(nsAWritableString& aNodeName);
  NS_IMETHOD    GetNodeValue(nsAWritableString& aNodeValue);
  NS_IMETHOD    SetNodeValue(const nsAReadableString& aNodeValue);
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
  NS_IMETHOD    GetPrefix(nsAWritableString& aPrefix);
  NS_IMETHOD    SetPrefix(const nsAReadableString& aPrefix);
  NS_IMETHOD    GetNamespaceURI(nsAWritableString& aNamespaceURI);
  NS_IMETHOD    GetLocalName(nsAWritableString& aLocalName);
  NS_IMETHOD    Normalize();
  NS_IMETHOD    Supports(const nsAReadableString& aFeature, const nsAReadableString& aVersion,
                         PRBool* aReturn);

  // interface nsIScriptObjectOwner
  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void* aScriptObject);

  // interface nsIContent
  NS_IMETHOD NormalizeAttributeString(const nsAReadableString& aStr, 
                                      nsINodeInfo*& aNodeInfo)
    {
      aNodeInfo = nsnull;
      return NS_OK; 
    }
  NS_IMETHOD GetDocument(nsIDocument*& aResult) const
    { return mInner.GetDocument(aResult); }
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers)
    { return mInner.SetDocument(aDocument, aDeep, aCompileEventHandlers); }
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
  NS_IMETHOD GetNodeInfo(nsINodeInfo*& aResult) const
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
                          const nsAReadableString& aValue,
                          PRBool aNotify)
    { return NS_OK; }
  NS_IMETHOD SetAttribute(nsINodeInfo* aNodeInfo,
                          const nsAReadableString& aValue,
                          PRBool aNotify)
    { return NS_OK; }
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, 
                          nsAWritableString& aResult) const
    { return NS_CONTENT_ATTR_NOT_THERE; }
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, 
                          nsIAtom*& aPrefix, nsAWritableString& aResult) const
    { return NS_CONTENT_ATTR_NOT_THERE; }
  NS_IMETHOD UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute, 
                            PRBool aNotify)
    { return NS_OK; }
  NS_IMETHOD GetAttributeNameAt(PRInt32 aIndex,
                                PRInt32& aNameSpaceID, 
                                nsIAtom*& aName,
                                nsIAtom*& aPrefix) const
    { 
      aName = nsnull;
      aPrefix = nsnull;
      return NS_ERROR_ILLEGAL_VALUE;
    }
  NS_IMETHOD GetAttributeCount(PRInt32& aCountResult) const
    { 
      aCountResult = 0;
      return NS_OK;
    }
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const
    { return mInner.List(out, aIndent); }
  NS_IMETHOD DumpContent(FILE* out = stdout, PRInt32 aIndent = 0,PRBool aDumpAll=PR_TRUE) const 
  {
    return mInner.DumpContent(out, aIndent,aDumpAll); 
  }
  NS_IMETHOD BeginConvertToXIF(nsIXIFConverter* aConverter) const
    { return NS_OK; }
  NS_IMETHOD ConvertContentToXIF(nsIXIFConverter* aConverter) const
    { return NS_OK; }
  NS_IMETHOD FinishConvertToXIF(nsIXIFConverter* aConverter) const
    { return NS_OK; }
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus* aEventStatus)
    {
      NS_ENSURE_ARG_POINTER(aEventStatus);
      *aEventStatus = nsEventStatus_eIgnore;
      return NS_OK;
    }

  NS_IMETHOD GetContentID(PRUint32* aID) {
    *aID = 0;
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD SetContentID(PRUint32 aID) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD RangeAdd(nsIDOMRange& aRange)
    {  return mInner.RangeAdd(aRange); } 
  NS_IMETHOD RangeRemove(nsIDOMRange& aRange)
    {  return mInner.RangeRemove(aRange); }                                                                        
  NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const 
    {  return mInner.GetRangeList(aResult); }   
  NS_IMETHOD SetFocus(nsIPresContext* aContext) {
    return mInner.SetFocus(aContext);
  }
  NS_IMETHOD RemoveFocus(nsIPresContext* aContext) {
    return mInner.RemoveFocus(aContext);
  }
  NS_IMETHOD GetBindingParent(nsIContent** aContent) {
    return mInner.GetBindingParent(aContent);
  }

  NS_IMETHOD SetBindingParent(nsIContent* aParent) {
    return mInner.SetBindingParent(aParent);
  }
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const {
    if (!aResult) {
      return NS_ERROR_NULL_POINTER;
    }
#ifdef DEBUG
    *aResult = sizeof(*this);
#else
    *aResult = 0;
#endif
    return NS_OK;
  }

protected:
  nsGenericContainerElement mInner;
  void* mScriptObject;
  nsIDocument* mOwnerDocument;
};

nsresult
NS_NewDocumentFragment(nsIDOMDocumentFragment** aInstancePtrResult,
                       nsIDocument* aOwnerDocument)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  nsCOMPtr<nsINodeInfoManager> nimgr;
  nsCOMPtr<nsINodeInfo> nodeInfo;

  nsresult rv;

  if (aOwnerDocument) {
    rv = aOwnerDocument->GetNodeInfoManager(*getter_AddRefs(nimgr));
  } else {
    rv = nsNodeInfoManager::GetAnonymousManager(*getter_AddRefs(nimgr));
  }
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nimgr->GetNodeInfo(NS_ConvertASCIItoUCS2("#document-fragment"),
                          nsnull, kNameSpaceID_None,
                          *getter_AddRefs(nodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  nsDocumentFragment* it = new nsDocumentFragment(aOwnerDocument, nodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIDOMDocumentFragment), 
                            (void**) aInstancePtrResult);
}

nsDocumentFragment::nsDocumentFragment(nsIDocument* aOwnerDocument,
                                       nsINodeInfo *aNodeInfo)
{
  NS_INIT_REFCNT();
  mInner.Init(this, aNodeInfo);
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
  if (aIID.Equals(NS_GET_IID(nsISupports))) {
    nsIDOMDocumentFragment* tmp = this;
    nsISupports* tmp2 = tmp;
    *aInstancePtr = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMDocumentFragment))) {
    nsIDOMDocumentFragment* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMNode))) {
    nsIDOMDocumentFragment* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIScriptObjectOwner))) {
    nsIScriptObjectOwner* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIContent))) {
    nsIContent* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP    
nsDocumentFragment::GetNodeName(nsAWritableString& aNodeName)
{
  aNodeName.Assign(NS_ConvertASCIItoUCS2("#document-fragment"));
  return NS_OK;
}

NS_IMETHODIMP    
nsDocumentFragment::GetNodeValue(nsAWritableString& aNodeValue)
{
  aNodeValue.Truncate();
  return NS_OK;
}

NS_IMETHODIMP    
nsDocumentFragment::SetNodeValue(const nsAReadableString& aNodeValue)
{
  // The node value can't be modified
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
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
    return mOwnerDocument->QueryInterface(NS_GET_IID(nsIDOMDocument), (void **)aOwnerDocument);
  }
  else {
    *aOwnerDocument = nsnull;
    return NS_OK;
  }
}

NS_IMETHODIMP
nsDocumentFragment::GetNamespaceURI(nsAWritableString& aNamespaceURI)
{ 
  aNamespaceURI.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentFragment::GetPrefix(nsAWritableString& aPrefix)
{
  aPrefix.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsDocumentFragment::SetPrefix(const nsAReadableString& aPrefix)
{
  return NS_ERROR_DOM_NAMESPACE_ERR;
}

NS_IMETHODIMP
nsDocumentFragment::GetLocalName(nsAWritableString& aLocalName)
{
  return GetNodeName(aLocalName);
}

NS_IMETHODIMP    
nsDocumentFragment::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsDocumentFragment* it;
  it = new nsDocumentFragment(mOwnerDocument, mInner.mNodeInfo);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
//XXX  mInner.CopyInnerTo(this, &it->mInner);
  return it->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aReturn);
}

NS_IMETHODIMP
nsDocumentFragment::Normalize()
{
  // Nothing to do here yet
  return NS_OK;
}


NS_IMETHODIMP
nsDocumentFragment::Supports(const nsAReadableString& aFeature, const nsAReadableString& aVersion,
                             PRBool* aReturn)
{
  return nsGenericElement::InternalSupports(aFeature, aVersion, aReturn);
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

