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


class nsDocumentFragment : public nsGenericContainerElement,
                           public nsIDOMDocumentFragment
{
public:
  nsDocumentFragment(nsIDocument* aOwnerDocument);
  virtual ~nsDocumentFragment();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // interface nsIDOMDocumentFragment
  NS_IMETHOD    GetNodeName(nsAWritableString& aNodeName)
  { return nsGenericContainerElement::GetNodeName(aNodeName); }
  NS_IMETHOD    GetNodeValue(nsAWritableString& aNodeValue)
  { return nsGenericContainerElement::GetNodeValue(aNodeValue); }
  NS_IMETHOD    SetNodeValue(const nsAReadableString& aNodeValue)
  { return nsGenericContainerElement::SetNodeValue(aNodeValue); }
  NS_IMETHOD    GetNodeType(PRUint16* aNodeType);
  NS_IMETHOD    GetParentNode(nsIDOMNode** aParentNode)
  { return nsGenericContainerElement::GetParentNode(aParentNode); }
  NS_IMETHOD    GetChildNodes(nsIDOMNodeList** aChildNodes)
  { return nsGenericContainerElement::GetChildNodes(aChildNodes); }
  NS_IMETHOD    GetFirstChild(nsIDOMNode** aFirstChild)
  { return nsGenericContainerElement::GetFirstChild(aFirstChild); }
  NS_IMETHOD    GetLastChild(nsIDOMNode** aLastChild)
  { return nsGenericContainerElement::GetLastChild(aLastChild); }
  NS_IMETHOD    GetPreviousSibling(nsIDOMNode** aPreviousSibling)
  { return nsGenericContainerElement::GetPreviousSibling(aPreviousSibling); }
  NS_IMETHOD    GetNextSibling(nsIDOMNode** aNextSibling)
  { return nsGenericContainerElement::GetNextSibling(aNextSibling); }
  NS_IMETHOD    GetAttributes(nsIDOMNamedNodeMap** aAttributes)
    {
      *aAttributes = nsnull;
      return NS_OK;
    }
  NS_IMETHOD    GetOwnerDocument(nsIDOMDocument** aOwnerDocument);
  NS_IMETHOD    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, 
                             nsIDOMNode** aReturn)
  { return nsGenericContainerElement::InsertBefore(aNewChild, aRefChild,
                                                   aReturn); }
  NS_IMETHOD    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, 
                             nsIDOMNode** aReturn)
  { return nsGenericContainerElement::ReplaceChild(aNewChild, aOldChild,
                                                   aReturn); }
  NS_IMETHOD    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
  { return nsGenericContainerElement::RemoveChild(aOldChild, aReturn); }
  NS_IMETHOD    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
  { return nsGenericContainerElement::AppendChild(aNewChild, aReturn); }
  NS_IMETHOD    HasChildNodes(PRBool* aReturn)
  { return nsGenericContainerElement::HasChildNodes(aReturn); }
  NS_IMETHOD    HasAttributes(PRBool* aReturn)
  { return nsGenericContainerElement::HasAttributes(aReturn); }
  NS_IMETHOD    CloneNode(PRBool aDeep, nsIDOMNode** aReturn);
  NS_IMETHOD    GetPrefix(nsAWritableString& aPrefix)
  { return nsGenericContainerElement::GetPrefix(aPrefix); }
  NS_IMETHOD    SetPrefix(const nsAReadableString& aPrefix);
  NS_IMETHOD    GetNamespaceURI(nsAWritableString& aNamespaceURI)
  { return nsGenericContainerElement::GetNamespaceURI(aNamespaceURI); }
  NS_IMETHOD    GetLocalName(nsAWritableString& aLocalName)
  { return nsGenericContainerElement::GetLocalName(aLocalName); }
  NS_IMETHOD    Normalize()
  { return nsGenericContainerElement::Normalize(); }
  NS_IMETHOD    IsSupported(const nsAReadableString& aFeature,
                            const nsAReadableString& aVersion,
                            PRBool* aReturn)
  { return nsGenericContainerElement::IsSupported(aFeature, aVersion,
                                                  aReturn); }

  // interface nsIScriptObjectOwner
  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);

  NS_IMETHOD SetParent(nsIContent* aParent)
    { return NS_OK; }
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
  nsCOMPtr<nsIDocument> mOwnerDocument;
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

  nsDocumentFragment* it = new nsDocumentFragment(aOwnerDocument);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  rv = it->Init(nodeInfo);

  if (NS_FAILED(rv)) {
    delete it;

    return rv;
  }

  *aInstancePtrResult = NS_STATIC_CAST(nsIDOMDocumentFragment *, it);

  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}

nsDocumentFragment::nsDocumentFragment(nsIDocument* aOwnerDocument)
{
  mOwnerDocument = aOwnerDocument;
}

nsDocumentFragment::~nsDocumentFragment()
{
}
 
NS_IMPL_ADDREF(nsDocumentFragment)
NS_IMPL_RELEASE(nsDocumentFragment)


NS_INTERFACE_MAP_BEGIN(nsDocumentFragment)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentFragment)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIContent)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectOwner)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContent)
NS_INTERFACE_MAP_END


NS_IMETHODIMP    
nsDocumentFragment::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::DOCUMENT_FRAGMENT_NODE;
  return NS_OK;
}

NS_IMETHODIMP    
nsDocumentFragment::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  NS_ENSURE_ARG_POINTER(aOwnerDocument);

  if (!mOwnerDocument) {
    *aOwnerDocument = nsnull;
    return NS_OK;
  }

  return mOwnerDocument->QueryInterface(NS_GET_IID(nsIDOMDocument),
                                        (void **)aOwnerDocument);
}

NS_IMETHODIMP
nsDocumentFragment::SetPrefix(const nsAReadableString& aPrefix)
{
  return NS_ERROR_DOM_NAMESPACE_ERR;
}

NS_IMETHODIMP    
nsDocumentFragment::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsDocumentFragment* it;
  it = new nsDocumentFragment(mOwnerDocument);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIContent> kungFuDeathGrip(it);

//XXX  CopyInnerTo(this, &it->mInner); ??? Why is this commented out?

  nsresult rv = it->Init(mNodeInfo);

  if (NS_FAILED(rv)) {
    delete it;

    return rv;
  }    

  *aReturn = NS_STATIC_CAST(nsIDOMNode *, it);

  NS_ADDREF(*aReturn);

  return NS_OK;
}

NS_IMETHODIMP 
nsDocumentFragment::GetScriptObject(nsIScriptContext* aContext, 
                                    void** aScriptObject)
{
  nsresult res = NS_OK;

  *aScriptObject = nsnull;

  nsDOMSlots *slots = GetDOMSlots();

  if (slots && !mDOMSlots->mScriptObject) {
    nsCOMPtr<nsIDOMScriptObjectFactory> factory;

    res = GetScriptObjectFactory(getter_AddRefs(factory));
    if (NS_OK != res) {
      return res;
    }

    res = factory->NewScriptDocumentFragment(aContext, 
                                             NS_STATIC_CAST(nsIDOMDocumentFragment *, this), 
                                             mOwnerDocument,
                                             (void**)&slots->mScriptObject);
  }

  if (slots) {
    *aScriptObject = slots->mScriptObject;
  }

  return res;
}
