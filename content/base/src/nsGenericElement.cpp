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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsGenericElement.h"

#include "nsDOMAttribute.h"
#include "nsDOMAttributeMap.h"
#include "nsIAtom.h"
#include "nsINodeInfo.h"
#include "nsIDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMRange.h"
#include "nsIDOMText.h"
#include "nsIDOMEventReceiver.h"
#include "nsRange.h"
#include "nsIEventListenerManager.h"
#include "nsILinkHandler.h"
#include "nsIScriptGlobalObject.h"
#include "nsISizeOfHandler.h"
#include "nsISupportsArray.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsFrame.h"
#include "nsIPresShell.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsString.h"
#include "nsDOMEventsIIDs.h"
#include "nsIEventStateManager.h"
#include "nsDOMEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsDOMCSSDeclaration.h"
#include "nsINameSpaceManager.h"
#include "nsContentList.h"
#include "nsDOMError.h"
#include "nsScriptSecurityManager.h"
#include "nsIDOMMutationEvent.h"
#include "nsMutationEvent.h"

#include "nsIBindingManager.h"
#include "nsIXBLBinding.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMViewCSS.h"
#include "nsIXBLService.h"
#include "nsPIDOMWindow.h"
#include "nsEventListenerManager.h"
#include "nsIBoxObject.h"
#include "nsPIBoxObject.h"
#include "nsIDOMNSDocument.h"

#include "nsLayoutAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsLayoutUtils.h"
#include "nsIJSContextStack.h"

#include "nsIServiceManager.h"

//----------------------------------------------------------------------

nsChildContentList::nsChildContentList(nsIContent *aContent)
{
  // This reference is not reference-counted. The content
  // object tells us when its about to go away.
  mContent = aContent;
}

nsChildContentList::~nsChildContentList()
{
}

NS_IMETHODIMP
nsChildContentList::GetLength(PRUint32* aLength)
{
  if (nsnull != mContent) {
    PRInt32 length;
    mContent->ChildCount(length);
    *aLength = (PRUint32)length;
  }
  else {
    *aLength = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsChildContentList::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  nsIContent *content;
  nsresult res = NS_OK;

  if (nsnull != mContent) {
    mContent->ChildAt(aIndex, content);
    if (nsnull != content) {
      res = content->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)aReturn);
      NS_RELEASE(content);
    }
    else {
      *aReturn = nsnull;
    }
  }
  else {
    *aReturn = nsnull;
  }

  return res;
}

void
nsChildContentList::DropReference()
{
  mContent = nsnull;
}

//----------------------------------------------------------------------

nsCheapVoidArray::nsCheapVoidArray()
{
  mChildren = nsnull;
}

nsCheapVoidArray::~nsCheapVoidArray()
{
  if (!HasSingleChild()) {
    nsVoidArray* vector = GetChildVector();
    if (vector) {
      delete vector;
    }
  }
}

PRInt32
nsCheapVoidArray::Count() const
{
  if (HasSingleChild()) {
    return 1;
  }
  else {
    nsVoidArray* vector = GetChildVector();
    if (vector) {
      return vector->Count();
    }
  }

  return 0;
}

void*
nsCheapVoidArray::ElementAt(PRInt32 aIndex) const
{
  if (HasSingleChild()) {
    if (0 == aIndex) {
      return (void*)GetSingleChild();
    }
  }
  else {
    nsVoidArray* vector = GetChildVector();
    if (vector) {
      return vector->ElementAt(aIndex);
    }
  }

  return nsnull;
}

PRInt32
nsCheapVoidArray::IndexOf(void* aPossibleElement) const
{
  if (HasSingleChild()) {
    if (aPossibleElement == (void*)GetSingleChild()) {
      return 0;
    }
  }
  else {
    nsVoidArray* vector = GetChildVector();
    if (vector) {
      return vector->IndexOf(aPossibleElement);
    }
  }

  return -1;
}

PRBool
nsCheapVoidArray::InsertElementAt(void* aElement, PRInt32 aIndex)
{
  nsVoidArray* vector;
  if (HasSingleChild()) {
    vector = SwitchToVector();
  }
  else {
    vector = GetChildVector();
    if (!vector) {
      if (0 == aIndex) {
        SetSingleChild(aElement);
        return PR_TRUE;
      }
      else {
        return PR_FALSE;
      }
    }
  }

  return vector->InsertElementAt(aElement, aIndex);
}

PRBool
nsCheapVoidArray::ReplaceElementAt(void* aElement, PRInt32 aIndex)
{
  if (HasSingleChild()) {
    if (aIndex == 0) {
      SetSingleChild(aElement);
      return PR_TRUE;
    }
    else {
      return PR_FALSE;
    }
  }
  else {
    nsVoidArray* vector = GetChildVector();
    if (vector) {
      return vector->ReplaceElementAt(aElement, aIndex);
    }
    else {
      return PR_FALSE;
    }
  }
}

PRBool
nsCheapVoidArray::AppendElement(void* aElement)
{
  nsVoidArray* vector;
  if (HasSingleChild()) {
    vector = SwitchToVector();
  }
  else {
    vector = GetChildVector();
    if (!vector) {
      SetSingleChild(aElement);
      return PR_TRUE;
    }
  }

  return vector->AppendElement(aElement);
}

PRBool
nsCheapVoidArray::RemoveElement(void* aElement)
{
  if (HasSingleChild()) {
    if (aElement == GetSingleChild()) {
      SetSingleChild(nsnull);
      return PR_TRUE;
    }
  }
  else {
    nsVoidArray* vector = GetChildVector();
    if (vector) {
      return vector->RemoveElement(aElement);
    }
  }

  return PR_FALSE;
}

PRBool
nsCheapVoidArray::RemoveElementAt(PRInt32 aIndex)
{
  if (HasSingleChild()) {
    if (0 == aIndex) {
      SetSingleChild(nsnull);
      return PR_TRUE;
    }
  }
  else {
    nsVoidArray* vector = GetChildVector();
    if (vector) {
      return vector->RemoveElementAt(aIndex);
    }
  }

  return PR_FALSE;
}

void
nsCheapVoidArray::Compact()
{
  if (!HasSingleChild()) {
    nsVoidArray* vector = GetChildVector();
    if (vector) {
      vector->Compact();
    }
  }
}

void
nsCheapVoidArray::SetSingleChild(void* aChild)
{
  if (aChild)
    mChildren = (void*)(PtrBits(aChild) | 0x1);
  else
    mChildren = nsnull;
}

nsVoidArray*
nsCheapVoidArray::SwitchToVector()
{
  void* child = GetSingleChild();

  mChildren = (void*)new nsVoidArray();
  nsVoidArray* vector = GetChildVector();
  if (vector && child) {
    vector->AppendElement(child);
  }

  return vector;
}

//----------------------------------------------------------------------

// XXX Currently, the script object factory is global. The way we
// obtain it should, at least, be made thread-safe later. Ideally,
// we'd find a better way.
nsIDOMScriptObjectFactory* nsGenericElement::gScriptObjectFactory = nsnull;

static NS_DEFINE_IID(kDOMScriptObjectFactoryCID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

nsresult
nsGenericElement::GetScriptObjectFactory(nsIDOMScriptObjectFactory **aResult)
{
  nsresult result = NS_OK;

  if (nsnull == gScriptObjectFactory) {
    result = nsServiceManager::GetService(kDOMScriptObjectFactoryCID,
                                          NS_GET_IID(nsIDOMScriptObjectFactory),
                                          (nsISupports **)&gScriptObjectFactory);
    if (result != NS_OK) {
      return result;
    }
  }

  *aResult = gScriptObjectFactory;
  NS_ADDREF(gScriptObjectFactory);
  return result;
}

/* static */ void
nsGenericElement::Shutdown()
{
  NS_IF_RELEASE(gScriptObjectFactory); // assigns null
}

nsGenericElement::nsGenericElement() : mDocument(nsnull), mParent(nsnull),
                                       mNodeInfo(nsnull), mDOMSlots(nsnull),
                                       mContentID(0)
{
  NS_INIT_REFCNT();
}

nsGenericElement::~nsGenericElement()
{
  // pop any enclosed ranges out
  // nsRange::OwnerGone(mContent); not used for now
  if (mDOMSlots) {
    if (mDOMSlots->mChildNodes) {
      mDOMSlots->mChildNodes->DropReference();
      NS_RELEASE(mDOMSlots->mChildNodes);
      delete mDOMSlots->mRangeList;
    }
    if (mDOMSlots->mStyle) {
      mDOMSlots->mStyle->DropReference();
      NS_RELEASE(mDOMSlots->mStyle);
    }
    if (mDOMSlots->mAttributeMap) {
      mDOMSlots->mAttributeMap->DropReference();
      NS_RELEASE(mDOMSlots->mAttributeMap);
    }
    if (mDOMSlots->mListenerManager) {
      mDOMSlots->mListenerManager->SetListenerTarget(nsnull);
      NS_RELEASE(mDOMSlots->mListenerManager);
    }

    // XXX Should really be arena managed
    delete mDOMSlots;
    mDOMSlots = nsnull;
  }

  NS_IF_RELEASE(mNodeInfo);
}

nsDOMSlots *
nsGenericElement::GetDOMSlots()
{
  if (!mDOMSlots) {
    mDOMSlots = new nsDOMSlots;

    if (!mDOMSlots)
      return nsnull;

    mDOMSlots->mScriptObject = nsnull;
    mDOMSlots->mChildNodes = nsnull;
    mDOMSlots->mStyle = nsnull;
    mDOMSlots->mAttributeMap = nsnull;
    mDOMSlots->mRangeList = nsnull;
    mDOMSlots->mCapturer = nsnull;
    mDOMSlots->mListenerManager = nsnull;
    mDOMSlots->mBindingParent = nsnull;
  }

  return mDOMSlots;
}

void
nsGenericElement::MaybeClearDOMSlots()
{
  if (mDOMSlots &&
      !mDOMSlots->mScriptObject &&
      !mDOMSlots->mChildNodes &&
      !mDOMSlots->mStyle &&
      !mDOMSlots->mAttributeMap &&
      !mDOMSlots->mRangeList &&
      !mDOMSlots->mCapturer &&
      !mDOMSlots->mListenerManager &&
      !mDOMSlots->mBindingParent) {

    delete mDOMSlots;
    mDOMSlots = nsnull;
  }
}

nsresult
nsGenericElement::Init(nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG(aNodeInfo);

  mNodeInfo = aNodeInfo;
  NS_ADDREF(mNodeInfo);

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetNodeName(nsAWritableString& aNodeName)
{
  return mNodeInfo->GetQualifiedName(aNodeName);
}

NS_IMETHODIMP
nsGenericElement::GetLocalName(nsAWritableString& aLocalName)
{
  return mNodeInfo->GetLocalName(aLocalName);
}

NS_IMETHODIMP
nsGenericElement::GetNodeValue(nsAWritableString& aNodeValue)
{
  aNodeValue.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::SetNodeValue(const nsAReadableString& aNodeValue)
{
  // The node value can't be modified
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

NS_IMETHODIMP
nsGenericElement::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::ELEMENT_NODE;
  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetParentNode(nsIDOMNode** aParentNode)
{
  nsresult res = NS_OK;

  if (nsnull != mParent) {
    res = mParent->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)aParentNode);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Node");
  }
  else if (nsnull == mDocument) {
    *aParentNode = nsnull;
  }
  else {
    // If we don't have a parent, but we're in the document, we must
    // be the root node of the document. The DOM says that the root
    // is the document.
    res = mDocument->QueryInterface(NS_GET_IID(nsIDOMNode),
                                    (void**)aParentNode);
  }

  return res;
}

NS_IMETHODIMP
nsGenericElement::GetPreviousSibling(nsIDOMNode** aPrevSibling)
{
  nsIContent* sibling = nsnull;
  nsresult result = NS_OK;

  if (nsnull != mParent) {
    PRInt32 pos;
    mParent->IndexOf(this, pos);
    if (pos > -1 ) {
      mParent->ChildAt(--pos, sibling);
    }
  }
  else if (nsnull != mDocument) {
    // Nodes that are just below the document (their parent is the
    // document) need to go to the document to find their next sibling.
    PRInt32 pos;
    mDocument->IndexOf(this, pos);
    if (pos > -1 ) {
      mDocument->ChildAt(--pos, sibling);
    }
  }

  if (nsnull != sibling) {
    result = sibling->QueryInterface(NS_GET_IID(nsIDOMNode),
                                     (void**)aPrevSibling);
    NS_ASSERTION(NS_OK == result, "Must be a DOM Node");
    NS_RELEASE(sibling); // balance the AddRef in ChildAt()
  }
  else {
    *aPrevSibling = nsnull;
  }

  return result;
}

NS_IMETHODIMP
nsGenericElement::GetNextSibling(nsIDOMNode** aNextSibling)
{
  nsIContent* sibling = nsnull;
  nsresult result = NS_OK;

  if (nsnull != mParent) {
    PRInt32 pos;
    mParent->IndexOf(this, pos);
    if (pos > -1 ) {
      mParent->ChildAt(++pos, sibling);
    }
  }
  else if (nsnull != mDocument) {
    // Nodes that are just below the document (their parent is the
    // document) need to go to the document to find their next sibling.
    PRInt32 pos;
    mDocument->IndexOf(this, pos);
    if (pos > -1 ) {
      mDocument->ChildAt(++pos, sibling);
    }
  }

  if (nsnull != sibling) {
    result = sibling->QueryInterface(NS_GET_IID(nsIDOMNode),
                                     (void**)aNextSibling);
    NS_ASSERTION(NS_OK == result, "Must be a DOM Node");
    NS_RELEASE(sibling); // balance the AddRef in ChildAt()
  }
  else {
    *aNextSibling = nsnull;
  }

  return result;
}

NS_IMETHODIMP
nsGenericElement::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  // XXX Actually the owner document is the document in whose context
  // the element has been created. We should be able to get at it
  // whether or not we are attached to the document.
  if (nsnull != mDocument) {
    return mDocument->QueryInterface(NS_GET_IID(nsIDOMDocument),
                                     (void **)aOwnerDocument);
  }
  else {
    *aOwnerDocument = nsnull;
    return NS_OK;
  }
}

NS_IMETHODIMP
nsGenericElement::GetNamespaceURI(nsAWritableString& aNamespaceURI)
{
  return mNodeInfo->GetNamespaceURI(aNamespaceURI);
}

NS_IMETHODIMP
nsGenericElement::GetPrefix(nsAWritableString& aPrefix)
{
  return mNodeInfo->GetPrefix(aPrefix);
}

NS_IMETHODIMP
nsGenericElement::SetPrefix(const nsAReadableString& aPrefix)
{
  // XXX: Validate the prefix string!

  nsINodeInfo *newNodeInfo = nsnull;
  nsCOMPtr<nsIAtom> prefix;

  if (aPrefix.Length()) {
    prefix = dont_AddRef(NS_NewAtom(aPrefix));
    NS_ENSURE_TRUE(prefix, NS_ERROR_OUT_OF_MEMORY);
  }

  nsresult rv = mNodeInfo->PrefixChanged(prefix, newNodeInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_RELEASE(mNodeInfo);

  mNodeInfo = newNodeInfo;

  return NS_OK;
}

nsresult
nsGenericElement::InternalIsSupported(const nsAReadableString& aFeature,
                                      const nsAReadableString& aVersion,
                                      PRBool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = PR_FALSE;
  nsAutoString feature(aFeature);

  if (feature.EqualsWithConversion("XML", PR_TRUE) ||
      feature.EqualsWithConversion("HTML", PR_TRUE)) {
    if (!aVersion.Length() ||
        aVersion.Equals(NS_LITERAL_STRING("1.0")) ||
        aVersion.Equals(NS_LITERAL_STRING("2.0"))) {
      *aReturn = PR_TRUE;
    }
  } else if (feature.EqualsWithConversion("Views", PR_TRUE) ||
             feature.EqualsWithConversion("StyleSheets", PR_TRUE) ||
             feature.EqualsWithConversion("CSS", PR_TRUE) ||
//           feature.EqualsWithConversion("CSS2", PR_TRUE) ||
             feature.EqualsWithConversion("Events", PR_TRUE) ||
//           feature.EqualsWithConversion("UIEvents", PR_TRUE) ||
             feature.EqualsWithConversion("MouseEvents", PR_TRUE) ||
             feature.EqualsWithConversion("HTMLEvents", PR_TRUE) ||
             feature.EqualsWithConversion("Range", PR_TRUE)) {
    if (!aVersion.Length() || aVersion.Equals(NS_LITERAL_STRING("2.0"))) {
      *aReturn = PR_TRUE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::IsSupported(const nsAReadableString& aFeature,
                              const nsAReadableString& aVersion,
                              PRBool* aReturn)
{
  return InternalIsSupported(aFeature, aVersion, aReturn);
}

NS_IMETHODIMP
nsGenericElement::HasAttributes(PRBool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  PRInt32 attrCount = 0;

  GetAttributeCount(attrCount);

  *aReturn = !!attrCount;

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
  NS_PRECONDITION(nsnull != aAttributes, "null pointer argument");
  nsDOMSlots *slots = GetDOMSlots();

  if (nsnull == slots->mAttributeMap) {
    slots->mAttributeMap = new nsDOMAttributeMap(this);
    if (nsnull == slots->mAttributeMap) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(slots->mAttributeMap);
  }

  return slots->mAttributeMap->QueryInterface(NS_GET_IID(nsIDOMNamedNodeMap),
                                              (void **)aAttributes);
}

NS_IMETHODIMP
nsGenericElement::GetTagName(nsAWritableString& aTagName)
{
  aTagName.Truncate();
  if (mNodeInfo) {
    mNodeInfo->GetName(aTagName);
  }
  return NS_OK;
}

nsresult
nsGenericElement::GetAttribute(const nsAReadableString& aName,
                               nsAWritableString& aReturn)
{
  nsCOMPtr<nsINodeInfo> ni;
  NormalizeAttributeString(aName, *getter_AddRefs(ni));
  NS_ENSURE_TRUE(ni, NS_ERROR_FAILURE);

  PRInt32 nsid;
  nsCOMPtr<nsIAtom> nameAtom;

  ni->GetNamespaceID(nsid);
  ni->GetNameAtom(*getter_AddRefs(nameAtom));

  NS_STATIC_CAST(nsIContent *, this)->GetAttribute(nsid, nameAtom, aReturn);

  return NS_OK;
}

nsresult
nsGenericElement::SetAttribute(const nsAReadableString& aName,
                               const nsAReadableString& aValue)
{
  nsCOMPtr<nsINodeInfo> ni;
  NormalizeAttributeString(aName, *getter_AddRefs(ni));
  return NS_STATIC_CAST(nsIContent *, this)->SetAttribute(ni, aValue, PR_TRUE);
}

nsresult
nsGenericElement::RemoveAttribute(const nsAReadableString& aName)
{
  nsCOMPtr<nsINodeInfo> ni;
  NormalizeAttributeString(aName, *getter_AddRefs(ni));
  NS_ENSURE_TRUE(ni, NS_ERROR_FAILURE);

  PRInt32 nsid;
  nsCOMPtr<nsIAtom> tag;

  ni->GetNamespaceID(nsid);
  ni->GetNameAtom(*getter_AddRefs(tag));

  return UnsetAttribute(nsid, tag, PR_TRUE);
}

nsresult
nsGenericElement::GetAttributeNode(const nsAReadableString& aName,
                                   nsIDOMAttr** aReturn)
{
  if (nsnull == aReturn) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIDOMNamedNodeMap* map;
  nsresult result = GetAttributes(&map);

  *aReturn = nsnull;
  if (NS_OK == result) {
    nsIDOMNode* node;
    result = map->GetNamedItem(aName, &node);
    if ((NS_OK == result) && (nsnull != node)) {
      result = node->QueryInterface(NS_GET_IID(nsIDOMAttr), (void **)aReturn);
      NS_IF_RELEASE(node);
    }
    NS_RELEASE(map);
  }

  return result;
}

nsresult
nsGenericElement::SetAttributeNode(nsIDOMAttr* aAttribute,
                                   nsIDOMAttr** aReturn)
{
  if ((nsnull == aReturn) || (nsnull == aAttribute))  {
    return NS_ERROR_NULL_POINTER;
  }
  nsIDOMNamedNodeMap* map;
  nsresult result = GetAttributes(&map);

  *aReturn = nsnull;
  if (NS_OK == result) {
    nsIDOMNode *node, *returnNode;
    result = aAttribute->QueryInterface(NS_GET_IID(nsIDOMNode),
                                        (void **)&node);
    if (NS_OK == result) {
      result = map->SetNamedItem(node, &returnNode);
      if ((NS_OK == result) && (nsnull != returnNode)) {
        result = returnNode->QueryInterface(NS_GET_IID(nsIDOMAttr),
                                            (void **)aReturn);
        NS_IF_RELEASE(returnNode);
      }
      NS_RELEASE(node);
    }
    NS_RELEASE(map);
  }

  return result;
}

nsresult
nsGenericElement::RemoveAttributeNode(nsIDOMAttr* aAttribute,
                                      nsIDOMAttr** aReturn)
{
  if ((nsnull == aReturn) || (nsnull == aAttribute))  {
    return NS_ERROR_NULL_POINTER;
  }
  nsIDOMNamedNodeMap* map;
  nsresult result = GetAttributes(&map);

  *aReturn = nsnull;
  if (NS_OK == result) {
    nsAutoString name;

    result = aAttribute->GetName(name);
    if (NS_OK == result) {
      nsIDOMNode* node;
      result = map->RemoveNamedItem(name, &node);
      if ((NS_OK == result) && (nsnull != node)) {
        result = node->QueryInterface(NS_GET_IID(nsIDOMAttr),
                                      (void **)aReturn);
        NS_RELEASE(node);
      }
    }
    NS_RELEASE(map);
  }

  return result;
}

nsresult
nsGenericElement::GetElementsByTagName(const nsAReadableString& aTagname,
                                       nsIDOMNodeList** aReturn)
{
  nsCOMPtr<nsIAtom> nameAtom;

  nameAtom = dont_AddRef(NS_NewAtom(aTagname));

  nsContentList* list = new nsContentList(mDocument,
                                          nameAtom,
                                          kNameSpaceID_Unknown,
                                          this);

  if (!list) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return list->QueryInterface(NS_GET_IID(nsIDOMNodeList), (void **)aReturn);
}

nsresult
nsGenericElement::GetAttributeNS(const nsAReadableString& aNamespaceURI,
                                 const nsAReadableString& aLocalName,
                                 nsAWritableString& aReturn)
{
  nsCOMPtr<nsIAtom> name(dont_AddRef(NS_NewAtom(aLocalName)));
  PRInt32 nsid;

  nsCOMPtr<nsINodeInfoManager> nimgr;
  mNodeInfo->GetNodeInfoManager(*getter_AddRefs(nimgr));
  NS_ENSURE_TRUE(nimgr, NS_ERROR_FAILURE);

  nsCOMPtr<nsINameSpaceManager> nsmgr;
  nimgr->GetNamespaceManager(*getter_AddRefs(nsmgr));
  NS_ENSURE_TRUE(nsmgr, NS_ERROR_FAILURE);

  nsmgr->GetNameSpaceID(aNamespaceURI, nsid);

  if (nsid == kNameSpaceID_Unknown) {
    // Unkonwn namespace means no attr...

    aReturn.Truncate();
    return NS_OK;
  }

  NS_STATIC_CAST(nsIContent *, this)->GetAttribute(nsid, name, aReturn);

  return NS_OK;
}

nsresult
nsGenericElement::SetAttributeNS(const nsAReadableString& aNamespaceURI,
                                 const nsAReadableString& aQualifiedName,
                                 const nsAReadableString& aValue)
{
  nsCOMPtr<nsINodeInfoManager> nimgr;
  mNodeInfo->GetNodeInfoManager(*getter_AddRefs(nimgr));
  NS_ENSURE_TRUE(nimgr, NS_ERROR_FAILURE);

  nsCOMPtr<nsINodeInfo> ni;
  nsresult rv = nimgr->GetNodeInfo(aQualifiedName, aNamespaceURI,
                                   *getter_AddRefs(ni));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_STATIC_CAST(nsIContent *, this)->SetAttribute(ni, aValue, PR_TRUE);
}

nsresult
nsGenericElement::RemoveAttributeNS(const nsAReadableString& aNamespaceURI,
                                    const nsAReadableString& aLocalName)
{
  nsCOMPtr<nsIAtom> name(dont_AddRef(NS_NewAtom(aLocalName)));
  PRInt32 nsid;

  nsCOMPtr<nsINodeInfoManager> nimgr;
  mNodeInfo->GetNodeInfoManager(*getter_AddRefs(nimgr));
  NS_ENSURE_TRUE(nimgr, NS_ERROR_FAILURE);

  nsCOMPtr<nsINameSpaceManager> nsmgr;
  nimgr->GetNamespaceManager(*getter_AddRefs(nsmgr));
  NS_ENSURE_TRUE(nsmgr, NS_ERROR_FAILURE);

  nsmgr->GetNameSpaceID(aNamespaceURI, nsid);

  if (nsid == kNameSpaceID_Unknown) {
    // Unkonwn namespace means no attr...

    return NS_OK;
  }

  nsAutoString tmp;
  UnsetAttribute(nsid, name, PR_TRUE);

  return NS_OK;
}

nsresult
nsGenericElement::GetAttributeNodeNS(const nsAReadableString& aNamespaceURI,
                                     const nsAReadableString& aLocalName,
                                     nsIDOMAttr** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  nsIDOMNamedNodeMap* map;
  nsresult result = GetAttributes(&map);

  *aReturn = nsnull;
  if (NS_OK == result) {
    nsIDOMNode* node;
    result = map->GetNamedItemNS(aNamespaceURI, aLocalName, &node);
    if ((NS_OK == result) && (nsnull != node)) {
      result = node->QueryInterface(NS_GET_IID(nsIDOMAttr), (void **)aReturn);
      NS_IF_RELEASE(node);
    }
    NS_RELEASE(map);
  }

  return result;
}

nsresult
nsGenericElement::SetAttributeNodeNS(nsIDOMAttr* aNewAttr,
                                     nsIDOMAttr** aReturn)
{
  if ((nsnull == aReturn) || (nsnull == aNewAttr))  {
    return NS_ERROR_NULL_POINTER;
  }
  nsIDOMNamedNodeMap* map;
  nsresult result = GetAttributes(&map);

  *aReturn = nsnull;
  if (NS_OK == result) {
    nsIDOMNode *node, *returnNode;
    result = aNewAttr->QueryInterface(NS_GET_IID(nsIDOMNode), (void **)&node);
    if (NS_OK == result) {
      result = map->SetNamedItemNS(node, &returnNode);
      if ((NS_OK == result) && (nsnull != returnNode)) {
        result = returnNode->QueryInterface(NS_GET_IID(nsIDOMAttr),
                                            (void **)aReturn);
        NS_IF_RELEASE(returnNode);
      }
      NS_RELEASE(node);
    }
    NS_RELEASE(map);
  }

  return NS_OK;
}

nsresult
nsGenericElement::GetElementsByTagNameNS(const nsAReadableString& aNamespaceURI,
                                         const nsAReadableString& aLocalName,
                                         nsIDOMNodeList** aReturn)
{
  nsCOMPtr<nsIAtom> nameAtom(dont_AddRef(NS_NewAtom(aLocalName)));
  PRInt32 nameSpaceId = kNameSpaceID_Unknown;

  nsContentList* list = nsnull;

  if (!aNamespaceURI.Equals(NS_LITERAL_STRING("*"))) {
    nsCOMPtr<nsINodeInfoManager> nimgr;
    mNodeInfo->GetNodeInfoManager(*getter_AddRefs(nimgr));
    NS_ENSURE_TRUE(nimgr, NS_ERROR_FAILURE);

    nsCOMPtr<nsINameSpaceManager> nsmgr;
    nimgr->GetNamespaceManager(*getter_AddRefs(nsmgr));
    NS_ENSURE_TRUE(nsmgr, NS_ERROR_FAILURE);

    nsmgr->GetNameSpaceID(aNamespaceURI, nameSpaceId);

    if (nameSpaceId == kNameSpaceID_Unknown) {
      // Unkonwn namespace means no matches, we create an empty list...
      list = new nsContentList(mDocument, nsnull, kNameSpaceID_None);
      NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);
    }
  }

  if (!list) {
    list = new nsContentList(mDocument, nameAtom, nameSpaceId, this);
    NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);
  }

  return list->QueryInterface(NS_GET_IID(nsIDOMNodeList), (void **)aReturn);
}

nsresult
nsGenericElement::HasAttribute(const nsAReadableString& aName, PRBool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  nsCOMPtr<nsINodeInfo> ni;
  NormalizeAttributeString(aName, *getter_AddRefs(ni));
  NS_ENSURE_TRUE(ni, NS_ERROR_FAILURE);

  PRInt32 nsid;
  nsCOMPtr<nsIAtom> nameAtom;

  ni->GetNamespaceID(nsid);
  ni->GetNameAtom(*getter_AddRefs(nameAtom));

  nsAutoString tmp;
  nsresult rv = NS_STATIC_CAST(nsIContent *, this)->GetAttribute(nsid,
                                                                 nameAtom,
                                                                 tmp);

  *aReturn = rv == NS_CONTENT_ATTR_NOT_THERE ? PR_FALSE : PR_TRUE;

  return NS_OK;
}

nsresult
nsGenericElement::HasAttributeNS(const nsAReadableString& aNamespaceURI,
                                 const nsAReadableString& aLocalName,
                                 PRBool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  nsCOMPtr<nsIAtom> name(dont_AddRef(NS_NewAtom(aLocalName)));
  PRInt32 nsid;

  nsCOMPtr<nsINodeInfoManager> nimgr;
  mNodeInfo->GetNodeInfoManager(*getter_AddRefs(nimgr));
  NS_ENSURE_TRUE(nimgr, NS_ERROR_FAILURE);

  nsCOMPtr<nsINameSpaceManager> nsmgr;
  nimgr->GetNamespaceManager(*getter_AddRefs(nsmgr));
  NS_ENSURE_TRUE(nsmgr, NS_ERROR_FAILURE);

  nsmgr->GetNameSpaceID(aNamespaceURI, nsid);

  if (nsid == kNameSpaceID_Unknown) {
    // Unkonwn namespace means no attr...

    *aReturn = PR_FALSE;
    return NS_OK;
  }

  nsAutoString tmp;
  nsresult rv = NS_STATIC_CAST(nsIContent *, this)->GetAttribute(nsid, name,
                                                                 tmp);

  *aReturn = rv == NS_CONTENT_ATTR_NOT_THERE ? PR_FALSE : PR_TRUE;

  return NS_OK;
}

nsresult
nsGenericElement::JoinTextNodes(nsIContent* aFirst,
                                nsIContent* aSecond)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIDOMText> firstText(do_QueryInterface(aFirst, &rv));

  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDOMText> secondText(do_QueryInterface(aSecond, &rv));

    if (NS_SUCCEEDED(rv)) {
      nsAutoString str;

      rv = secondText->GetData(str);
      if (NS_SUCCEEDED(rv)) {
        rv = firstText->AppendData(str);
      }
    }
  }

  return rv;
}

nsresult
nsGenericElement::Normalize()
{
  nsresult result = NS_OK;
  PRInt32 index, count;

  ChildCount(count);
  for (index = 0; (index < count) && (NS_OK == result); index++) {
    nsCOMPtr<nsIContent> child;

    result = ChildAt(index, *getter_AddRefs(child));
    if (NS_FAILED(result)) {
      return result;
    }

    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(child);
    if (node) {
      PRUint16 nodeType;
      node->GetNodeType(&nodeType);

      switch (nodeType) {
        case nsIDOMNode::TEXT_NODE:

          if (index+1 < count) {
            nsCOMPtr<nsIContent> sibling;

            // Get the sibling. If it's also a text node, then
            // remove it from the tree and join the two text
            // nodes.
            result = ChildAt(index+1, *getter_AddRefs(sibling));
            if (NS_FAILED(result)) {
              return result;
            }

            nsCOMPtr<nsIDOMNode> siblingNode = do_QueryInterface(sibling);

            if (sibling) {
              PRUint16 siblingNodeType;
              siblingNode->GetNodeType(&siblingNodeType);

              if (siblingNodeType == nsIDOMNode::TEXT_NODE) {
                result = RemoveChildAt(index+1, PR_TRUE);
                if (NS_FAILED(result)) {
                  return result;
                }

                result = JoinTextNodes(child, sibling);
                if (NS_FAILED(result)) {
                  return result;
                }
                count--;
                index--;
              }
            }
          }
          break;

        case nsIDOMNode::ELEMENT_NODE:
          nsCOMPtr<nsIDOMElement> element = do_QueryInterface(child);

          if (element) {
            result = element->Normalize();
          }
          break;
      }
    }
  }

  return result;
}


nsresult
nsGenericElement::GetDocument(nsIDocument*& aResult) const
{
  NS_IF_ADDREF(mDocument);
  aResult = mDocument;
  return NS_OK;
}


void
nsGenericElement::SetDocumentInChildrenOf(nsIContent* aContent,
                                          nsIDocument* aDocument,
                                          PRBool aCompileEventHandlers)
{
  PRInt32 i, n;
  aContent->ChildCount(n);
  for (i = 0; i < n; i++) {
    nsIContent* child;
    aContent->ChildAt(i, child);
    if (nsnull != child) {
      child->SetDocument(aDocument, PR_TRUE, aCompileEventHandlers);
      NS_RELEASE(child);
    }
  }
}

nsresult
nsGenericElement::SetDocument(nsIDocument* aDocument, PRBool aDeep,
                              PRBool aCompileEventHandlers)
{
  if (aDocument != mDocument) {
    // If we were part of a document, make sure we get rid of the
    // script context reference to our script object so that our
    // script object can be freed (or collected).
    if (mDocument && mDOMSlots && mDOMSlots->mScriptObject) {
      nsCOMPtr<nsIScriptGlobalObject> globalObject;
      mDocument->GetScriptGlobalObject(getter_AddRefs(globalObject));
      if (globalObject) {
        nsCOMPtr<nsIScriptContext> context;
        if (NS_OK == globalObject->GetContext(getter_AddRefs(context)) && context) {
          context->RemoveReference((void *)&mDOMSlots->mScriptObject,
                                   mDOMSlots->mScriptObject);
        }
      }
    }

    if (mDocument && aDeep) {
      // Notify XBL- & nsIAnonymousContentCreator-generated
      // anonymous content that the document is changing.
      nsCOMPtr<nsIBindingManager> bindingManager;
      mDocument->GetBindingManager(getter_AddRefs(bindingManager));
      NS_ASSERTION(bindingManager, "No binding manager.");
      if (bindingManager) {
        bindingManager->ChangeDocumentFor(this, mDocument, aDocument);
      }

      nsCOMPtr<nsIDOMElement> domElement;
      QueryInterface(NS_GET_IID(nsIDOMElement), getter_AddRefs(domElement));

      if (domElement) {
        nsCOMPtr<nsIDOMNSDocument> nsDoc(do_QueryInterface(mDocument));
        nsDoc->SetBoxObjectFor(domElement, nsnull);
      }
    }

    mDocument = aDocument;

    // If we already have a script object and now we're being added
    // to a document, make sure that the script context adds a
    // reference to our script object. This will ensure that it
    // won't be freed (or collected) out from under us.
    if (mDocument && mDOMSlots && mDOMSlots->mScriptObject) {
      nsCOMPtr<nsIScriptGlobalObject> globalObject;
      mDocument->GetScriptGlobalObject(getter_AddRefs(globalObject));
      if (globalObject) {
        nsCOMPtr<nsIScriptContext> context;
        if (NS_OK == globalObject->GetContext(getter_AddRefs(context)) && context) {
          context->AddNamedReference((void *)&mDOMSlots->mScriptObject,
                                     mDOMSlots->mScriptObject,
                                     "nsGenericElement::mScriptObject");
        }
      }
    }
  }

  if (PR_TRUE == aDeep) {
    SetDocumentInChildrenOf(this, aDocument, aCompileEventHandlers);
  }

  return NS_OK;
}


nsresult
nsGenericElement::GetParent(nsIContent*& aResult) const
{
  aResult = mParent;
  NS_IF_ADDREF(aResult);
  return NS_OK;
}

nsresult
nsGenericElement::SetParent(nsIContent* aParent)
{
  mParent = aParent;
  return NS_OK;
}

nsresult
nsGenericElement::GetNameSpaceID(PRInt32& aNameSpaceID) const
{
  return mNodeInfo->GetNamespaceID(aNameSpaceID);
}

nsresult
nsGenericElement::GetTag(nsIAtom*& aResult) const
{
  return mNodeInfo->GetNameAtom(aResult);
}

nsresult
nsGenericElement::GetNodeInfo(nsINodeInfo*& aResult) const
{
  aResult = mNodeInfo;
  NS_IF_ADDREF(aResult);

  return NS_OK;
}


nsresult
nsGenericElement::HandleDOMEvent(nsIPresContext* aPresContext,
                                 nsEvent* aEvent,
                                 nsIDOMEvent** aDOMEvent,
                                 PRUint32 aFlags,
                                 nsEventStatus* aEventStatus)
{
  nsresult ret = NS_OK;
  PRBool retarget = PR_FALSE;
  nsCOMPtr<nsIDOMEventTarget> oldTarget;

  nsIDOMEvent* domEvent = nsnull;
  if (NS_EVENT_FLAG_INIT & aFlags) {
    if (!aDOMEvent) {
      aDOMEvent = &domEvent;
    }
    aEvent->flags = aFlags;
    aFlags &= ~(NS_EVENT_FLAG_CANT_BUBBLE | NS_EVENT_FLAG_CANT_CANCEL);
  }

  // Find out if we're anonymous.
  nsCOMPtr<nsIContent> bindingParent;
  GetBindingParent(getter_AddRefs(bindingParent));
  if (bindingParent) {
    // We're anonymous.  We may potentially need to retarget
    // our event if our parent is in a different scope.
    if (mParent) {
      nsCOMPtr<nsIContent> parentScope;
      mParent->GetBindingParent(getter_AddRefs(parentScope));
      if (parentScope != bindingParent)
        retarget = PR_TRUE;
    }
  }

  if (retarget) {
    if (!*aDOMEvent) {
      // We haven't made a DOMEvent yet.  Force making one now.
      nsCOMPtr<nsIEventListenerManager> listenerManager;
      if (NS_FAILED(ret = GetListenerManager(getter_AddRefs(listenerManager)))) {
        return ret;
      }
      nsAutoString empty;
      if (NS_FAILED(ret = listenerManager->CreateEvent(aPresContext, aEvent, empty, aDOMEvent)))
        return ret;
    }

    if (!*aDOMEvent) {
      return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(*aDOMEvent);
    if (!privateEvent) {
      return NS_ERROR_FAILURE;
    }

    (*aDOMEvent)->GetTarget(getter_AddRefs(oldTarget));

    PRBool hasOriginal;
    privateEvent->HasOriginalTarget(&hasOriginal);

    if (!hasOriginal) {
      privateEvent->SetOriginalTarget(oldTarget);
    }

    nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(mParent);
    privateEvent->SetTarget(target);
  }

  //Capturing stage evaluation
  //Always pass capturing up the tree before local evaulation
  if (NS_EVENT_FLAG_BUBBLE != aFlags) {
  // XXX: Bring on the more optimized version of capturing at some point
    //if (nsnull != mDOMSlots && nsnull != mDOMSlots->mCapturer) {
      //mDOMSlots->mCapturer->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, NS_EVENT_FLAG_CAPTURE, aEventStatus);
    //} else {
      // Node capturing stage
      if (mParent) {
        // Pass off to our parent.
        mParent->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, NS_EVENT_FLAG_CAPTURE, aEventStatus);
      } else {
        //Initiate capturing phase.  Special case first call to document
        if (nsnull != mDocument) {
          mDocument->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, NS_EVENT_FLAG_CAPTURE, aEventStatus);
        }
      }
    //}
  }

  if (retarget) {
    // The event originated beneath us, and we performed a retargeting.
    // We need to restore the original target of the event.
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(*aDOMEvent);
    if (privateEvent)
      privateEvent->SetTarget(oldTarget);
  }

  //Local handling stage
  if (mDOMSlots && mDOMSlots->mListenerManager && !(aEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH) &&
      !(NS_EVENT_FLAG_BUBBLE & aFlags && NS_EVENT_FLAG_CANT_BUBBLE & aEvent->flags)) {
    aEvent->flags |= aFlags;
    nsCOMPtr<nsIDOMEventTarget> curTarg(do_QueryInterface(NS_STATIC_CAST(nsIHTMLContent *, this)));
    mDOMSlots->mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, curTarg, aFlags, aEventStatus);
    aEvent->flags &= ~aFlags;
  }

  if (retarget) {
    // The event originated beneath us, and we need to perform a retargeting.
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(*aDOMEvent);
    if (privateEvent) {
      nsCOMPtr<nsIDOMEventTarget> parentTarget(do_QueryInterface(mParent));
      privateEvent->SetTarget(parentTarget);
    }
  }

  //Bubbling stage
  if (NS_EVENT_FLAG_CAPTURE != aFlags && mDocument) {
    if (mParent) {
      /*
       * If there's a parent we pass the event to the parent...
       */
      ret = mParent->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                    NS_EVENT_FLAG_BUBBLE, aEventStatus);
    } else {
      /*
       * If there's no parent but there is a document (i.e. this is the
       * root node) we pass the event to the document...
       */
      ret = mDocument->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                      NS_EVENT_FLAG_BUBBLE, aEventStatus);
    }
  }

  if (retarget) {
    // The event originated beneath us, and we performed a retargeting.
    // We need to restore the original target of the event.
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(*aDOMEvent);
    if (privateEvent)
      privateEvent->SetTarget(oldTarget);
  }

  if (NS_EVENT_FLAG_INIT & aFlags) {
    // We're leaving the DOM event loop so if we created a DOM event,
    // release here.
    if (nsnull != *aDOMEvent) {
      nsrefcnt rc;
      NS_RELEASE2(*aDOMEvent, rc);
      if (0 != rc) {
        // Okay, so someone in the DOM loop (a listener, JS object)
        // still has a ref to the DOM Event but the internal data
        // hasn't been malloc'd.  Force a copy of the data here so the
        // DOM Event is still valid.
        nsIPrivateDOMEvent *privateEvent;
        if (NS_OK == (*aDOMEvent)->QueryInterface(NS_GET_IID(nsIPrivateDOMEvent), (void**)&privateEvent)) {
          privateEvent->DuplicatePrivateData();
          NS_RELEASE(privateEvent);
        }
      }
    }
    aDOMEvent = nsnull;
  }

  return ret;
}

PRUint32
nsGenericElement::BaseSizeOf(nsISizeOfHandler *aSizer) const
{
  return 0;
}

NS_IMETHODIMP
nsGenericElement::GetContentID(PRUint32* aID)
{
  *aID = mContentID;

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::SetContentID(PRUint32 aID)
{
  mContentID = aID;

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::SetContainingNameSpace(nsINameSpace* aNameSpace)
{
  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetContainingNameSpace(nsINameSpace*& aNameSpace) const
{
  aNameSpace = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::MaybeTriggerAutoLink(nsIWebShell *aShell)
{
  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetID(nsIAtom*& aResult) const
{
  aResult = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetClasses(nsVoidArray& aArray) const
{
  aArray.Clear();
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGenericElement::HasClass(nsIAtom* aClass) const
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGenericElement::GetContentStyleRules(nsISupportsArray* aRules)
{
  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetInlineStyleRules(nsISupportsArray* aRules)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGenericElement::GetMappedAttributeImpact(const nsIAtom* aAttribute,
                                           PRInt32& aHint) const
{
  aHint = NS_STYLE_HINT_CONTENT;  // by default, never map attributes to style
  return NS_OK;
}


NS_IMETHODIMP
nsGenericElement::Compact()
{
  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::SetHTMLAttribute(nsIAtom* aAttribute,
                                   const nsHTMLValue& aValue,
                                   PRBool aNotify)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGenericElement::GetHTMLAttribute(nsIAtom* aAttribute,
                                   nsHTMLValue& aValue) const
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGenericElement::GetAttributeMappingFunctions(nsMapAttributesFunc& aFontMapFunc,
                                               nsMapAttributesFunc& aMapFunc) const
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGenericElement::AttributeToString(nsIAtom* aAttribute,
                                    const nsHTMLValue& aValue,
                                    nsAWritableString& aResult) const
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGenericElement::StringToAttribute(nsIAtom* aAttribute,
                                    const nsAReadableString& aValue,
                                    nsHTMLValue& aResult)
{
  return NS_CONTENT_ATTR_NOT_THERE;
}

NS_IMETHODIMP
nsGenericElement::GetBaseURL(nsIURI*& aBaseURL) const
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGenericElement::GetBaseTarget(nsAWritableString& aBaseTarget) const
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsGenericElement::RangeAdd(nsIDOMRange& aRange)
{
  if (!mDOMSlots)
    GetDOMSlots();

  // lazy allocation of range list
  if (!mDOMSlots->mRangeList) {
    mDOMSlots->mRangeList = new nsVoidArray();
  }
  if (!mDOMSlots->mRangeList) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Make sure we don't add a range that is already
  // in the list!
  PRInt32 i = mDOMSlots->mRangeList->IndexOf(&aRange);
  if (i >= 0) {
    // Range is already in the list, so there
    // is nothing to do!
    return NS_OK;
  }

  // dont need to addref - this call is made by the range object itself
  PRBool rv = mDOMSlots->mRangeList->AppendElement(&aRange);
  if (rv)
    return NS_OK;

  return NS_ERROR_FAILURE;
}


nsresult
nsGenericElement::RangeRemove(nsIDOMRange& aRange)
{
  if (mDOMSlots && mDOMSlots->mRangeList) {
    // dont need to release - this call is made by the range object itself
    PRBool rv = mDOMSlots->mRangeList->RemoveElement(&aRange);
    if (rv) {
      if (mDOMSlots->mRangeList->Count() == 0) {
        delete mDOMSlots->mRangeList;
        mDOMSlots->mRangeList = nsnull;
        MaybeClearDOMSlots();
      }
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}


nsresult
nsGenericElement::GetRangeList(nsVoidArray*& aResult) const
{
  if (mDOMSlots && mDOMSlots->mRangeList) {
    aResult = mDOMSlots->mRangeList;
  }
  else {
    aResult = nsnull;
  }
  return NS_OK;
}

nsresult
nsGenericElement::SetFocus(nsIPresContext* aPresContext)
{
  nsAutoString disabled;
  if (NS_CONTENT_ATTR_HAS_VALUE == NS_STATIC_CAST(nsIContent *, this)->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::disabled, disabled)) {
    return NS_OK;
  }
 
  nsIEventStateManager* esm;
  if (NS_OK == aPresContext->GetEventStateManager(&esm)) {
    
    esm->SetContentState(this, NS_EVENT_STATE_FOCUS);
    NS_RELEASE(esm);
  }
  
  return NS_OK;
}

nsresult
nsGenericElement::RemoveFocus(nsIPresContext* aPresContext)
{
  return NS_OK;
}

nsresult
nsGenericElement::GetBindingParent(nsIContent** aContent)
{
  *aContent = mDOMSlots ? mDOMSlots->mBindingParent : nsnull;
  NS_IF_ADDREF(*aContent);
  return NS_OK;
}

nsresult
nsGenericElement::SetBindingParent(nsIContent* aParent)
{
  if (!mDOMSlots)
    GetDOMSlots();

  mDOMSlots->mBindingParent = aParent; // Weak, so no addref happens.

  if (aParent) {
    PRInt32 count;
    ChildCount(count);
    for (PRInt32 i = 0; i < count; i++) {
      nsCOMPtr<nsIContent> child;
      ChildAt(i, *getter_AddRefs(child));
      child->SetBindingParent(aParent);
    }
  }

  return NS_OK;
}


//----------------------------------------------------------------------

nsresult
nsGenericElement::RenderFrame(nsIPresContext* aPresContext)
{
  nsPoint offset;
  nsRect bounds;

  // Trigger damage repairs for each frame that maps the given content
  PRInt32 i, n;
  n = mDocument->GetNumberOfShells();
  for (i = 0; i < n; i++) {
    nsIPresShell* shell;
    shell = mDocument->GetShellAt(i);
    nsIFrame* frame;
    shell->GetPrimaryFrameFor(this, &frame);
    while (nsnull != frame) {
      nsIViewManager* vm;
      nsIView* view;

      // Determine damaged area and tell view manager to redraw it
      frame->GetRect(bounds);
      bounds.x = bounds.y = 0;

      // XXX We should tell the frame the damage area and let it invalidate
      // itself. Add some API calls to nsIFrame to allow a caller to invalidate
      // parts of the frame...
      frame->GetOffsetFromView(aPresContext, offset, &view);
      view->GetViewManager(vm);
      bounds.x += offset.x;
      bounds.y += offset.y;

      vm->UpdateView(view, bounds, NS_VMREFRESH_IMMEDIATE);
      NS_RELEASE(vm);

      // If frame has a next-in-flow, repaint it too
      frame->GetNextInFlow(&frame);
    }
    NS_RELEASE(shell);
  }

  return NS_OK;
}

//----------------------------------------------------------------------

// nsIScriptObjectOwner implementation

nsresult
nsGenericElement::GetScriptObject(nsIScriptContext* aContext,
                                  void** aScriptObject)
{
  nsresult res = NS_OK;
  nsDOMSlots *slots = GetDOMSlots();

  if (!slots->mScriptObject) {
    nsIDOMScriptObjectFactory *factory;

    res = GetScriptObjectFactory(&factory);
    if (NS_OK != res) {
      return res;
    }

    nsAutoString tag;
    mNodeInfo->GetName(tag);
    res = factory->NewScriptElement(tag, aContext,
                                    NS_STATIC_CAST(nsIHTMLContent *, this),
                                    mParent ? (nsISupports*)mParent : (nsISupports*)mDocument,
                                    (void**)&slots->mScriptObject);
    NS_RELEASE(factory);

    NS_WARN_IF_FALSE(slots->mScriptObject,
                     "Eeek! Cound't create script object!");

    if (mDocument && slots->mScriptObject) {
      aContext->AddNamedReference((void *)&slots->mScriptObject,
                                  slots->mScriptObject,
                                  "nsGenericElement::mScriptObject");

      // See if we have a frame.
      nsCOMPtr<nsIPresShell> shell = getter_AddRefs(mDocument->GetShellAt(0));
      if (shell) {
        nsIFrame* frame;
        shell->GetPrimaryFrameFor(this, &frame);
        if (!frame) {
          // We must ensure that the XBL Binding is installed before we hand
          // back this object.
          nsCOMPtr<nsIBindingManager> bindingManager;
          mDocument->GetBindingManager(getter_AddRefs(bindingManager));
          nsCOMPtr<nsIXBLBinding> binding;
          bindingManager->GetBinding(this, getter_AddRefs(binding));
          if (!binding) {
            nsCOMPtr<nsIScriptGlobalObject> global;
            mDocument->GetScriptGlobalObject(getter_AddRefs(global));
            nsCOMPtr<nsIDOMViewCSS> viewCSS(do_QueryInterface(global));
            if (viewCSS) {
              nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl;
              nsAutoString empty;
              nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(NS_STATIC_CAST(nsIHTMLContent *, this)));
              viewCSS->GetComputedStyle(elt, empty, getter_AddRefs(cssDecl));
              if (cssDecl) {
                nsAutoString behavior; behavior.Assign(NS_LITERAL_STRING("-moz-binding"));
                nsAutoString value;
                cssDecl->GetPropertyValue(behavior, value);
                if (!value.IsEmpty()) {
                  // We have a binding that must be installed.
                  nsresult rv;
                  PRBool dummy;
                  NS_WITH_SERVICE(nsIXBLService, xblService, "@mozilla.org/xbl;1", &rv);
                  xblService->LoadBindings(this, value, PR_FALSE, getter_AddRefs(binding), &dummy);
                  if (binding) {
                    binding->ExecuteAttachedHandler();
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  *aScriptObject = slots->mScriptObject;
  return res;
}

nsresult
nsGenericElement::SetScriptObject(void *aScriptObject)
{
  nsDOMSlots *slots = GetDOMSlots();

  slots->mScriptObject = aScriptObject;

  if (!aScriptObject) {
    if (slots->mListenerManager) {
      slots->mListenerManager->RemoveAllListeners(PR_TRUE);
    }
    MaybeClearDOMSlots();
  }

  return NS_OK;
}

//----------------------------------------------------------------------

nsresult
nsGenericElement::GetListenerManager(nsIEventListenerManager** aResult)
{
  nsDOMSlots *slots = GetDOMSlots();

  if (nsnull != slots->mListenerManager) {
    NS_ADDREF(slots->mListenerManager);
    *aResult = slots->mListenerManager;
    return NS_OK;
  }
  nsresult rv = NS_NewEventListenerManager(aResult);
  if (NS_OK == rv) {
    slots->mListenerManager = *aResult;
    NS_ADDREF(slots->mListenerManager);
    slots->mListenerManager->SetListenerTarget(NS_STATIC_CAST(nsIHTMLContent *, this));
  }
  return rv;
}

//----------------------------------------------------------------------

// nsIJSScriptObject implementation

PRBool
nsGenericElement::AddProperty(JSContext *aContext, JSObject *aObj, jsval aID,
                              jsval *aVp)
{
  return PR_TRUE;
}

PRBool
nsGenericElement::DeleteProperty(JSContext *aContext, JSObject *aObj,
                                 jsval aID, jsval *aVp)
{
  return PR_TRUE;
}

PRBool
nsGenericElement::GetProperty(JSContext *aContext, JSObject *aObj, jsval aID,
                              jsval *aVp)
{
  return PR_TRUE;
}

PRBool
nsGenericElement::SetProperty(JSContext *aContext, JSObject *aObj, jsval aID,
                              jsval *aVp)
{
  if (JS_TypeOfValue(aContext, *aVp) == JSTYPE_FUNCTION &&
      JSVAL_IS_STRING(aID)) {
    nsAutoString propName, prefix;
    propName.Assign(NS_REINTERPRET_CAST(const PRUnichar*, JS_GetStringChars(JS_ValueToString(aContext, aID))));
    if (propName.Length() > 2)
      prefix.Assign(propName.GetUnicode(), 2);
    if (prefix.Equals(NS_LITERAL_STRING("on"))) {
      nsCOMPtr<nsIAtom> atom = getter_AddRefs(NS_NewAtom(propName));
      nsIEventListenerManager *manager = nsnull;

      if (atom.get() == nsLayoutAtoms::onmousedown || atom.get() == nsLayoutAtoms::onmouseup || atom.get() ==  nsLayoutAtoms::onclick ||
         atom.get() == nsLayoutAtoms::onmouseover || atom.get() == nsLayoutAtoms::onmouseout) {
        if (NS_OK == GetListenerManager(&manager)) {
          nsCOMPtr<nsIScriptContext> mScriptCX;
          if (NS_FAILED(nsLayoutUtils::GetStaticScriptContext(aContext, (JSObject*)GetDOMSlots()->mScriptObject, getter_AddRefs(mScriptCX))) ||
              NS_OK != manager->RegisterScriptEventListener(mScriptCX, this, atom, kIDOMMouseListenerIID)) {
            NS_RELEASE(manager);
            return PR_FALSE;
          }
        }
      }
      else if (atom.get() == nsLayoutAtoms::onkeydown || atom.get() == nsLayoutAtoms::onkeyup || atom.get() == nsLayoutAtoms::onkeypress) {
        if (NS_OK == GetListenerManager(&manager)) {
          nsCOMPtr<nsIScriptContext> mScriptCX;
          if (NS_FAILED(nsLayoutUtils::GetStaticScriptContext(aContext, (JSObject*)GetDOMSlots()->mScriptObject, getter_AddRefs(mScriptCX))) ||
              NS_OK != manager->RegisterScriptEventListener(mScriptCX, this, atom, kIDOMKeyListenerIID)) {
            NS_RELEASE(manager);
            return PR_FALSE;
          }
        }
      }
      else if (atom.get() == nsLayoutAtoms::onmousemove) {
        if (NS_OK == GetListenerManager(&manager)) {
          nsCOMPtr<nsIScriptContext> mScriptCX;
          if (NS_FAILED(nsLayoutUtils::GetStaticScriptContext(aContext, (JSObject*)GetDOMSlots()->mScriptObject, getter_AddRefs(mScriptCX))) ||
              NS_OK != manager->RegisterScriptEventListener(mScriptCX, this, atom, kIDOMMouseMotionListenerIID)) {
            NS_RELEASE(manager);
            return PR_FALSE;
          }
        }
      }
      else if (atom.get() == nsLayoutAtoms::onfocus || atom.get() == nsLayoutAtoms::onblur) {
        if (NS_OK == GetListenerManager(&manager)) {
          nsCOMPtr<nsIScriptContext> mScriptCX;
          if (NS_FAILED(nsLayoutUtils::GetStaticScriptContext(aContext, (JSObject*)GetDOMSlots()->mScriptObject, getter_AddRefs(mScriptCX))) ||
              NS_OK != manager->RegisterScriptEventListener(mScriptCX, this, atom, kIDOMFocusListenerIID)) {
            NS_RELEASE(manager);
            return PR_FALSE;
          }
        }
      }
      else if (atom.get() == nsLayoutAtoms::onsubmit || atom.get() == nsLayoutAtoms::onreset || atom.get() == nsLayoutAtoms::onchange ||
               atom.get() == nsLayoutAtoms::onselect) {
        if (NS_OK == GetListenerManager(&manager)) {
          nsCOMPtr<nsIScriptContext> mScriptCX;
          if (NS_FAILED(nsLayoutUtils::GetStaticScriptContext(aContext, (JSObject*)GetDOMSlots()->mScriptObject, getter_AddRefs(mScriptCX))) ||
              NS_OK != manager->RegisterScriptEventListener(mScriptCX, this, atom, kIDOMFormListenerIID)) {
            NS_RELEASE(manager);
            return PR_FALSE;
          }
        }
      }
      else if (atom.get() == nsLayoutAtoms::onload || atom.get() == nsLayoutAtoms::onunload || atom.get() == nsLayoutAtoms::onabort ||
               atom.get() == nsLayoutAtoms::onerror) {
        if (NS_OK == GetListenerManager(&manager)) {
          nsCOMPtr<nsIScriptContext> mScriptCX;
          if (NS_FAILED(nsLayoutUtils::GetStaticScriptContext(aContext, (JSObject*)GetDOMSlots()->mScriptObject, getter_AddRefs(mScriptCX))) ||
              NS_OK != manager->RegisterScriptEventListener(mScriptCX, this, atom, kIDOMLoadListenerIID)) {
            NS_RELEASE(manager);
            return PR_FALSE;
          }
        }
      }
      else if (atom.get() == nsLayoutAtoms::onpaint || atom.get() == nsLayoutAtoms::onresize ||
               atom.get() == nsLayoutAtoms::onscroll) {
        if (NS_OK == GetListenerManager(&manager)) {
          nsCOMPtr<nsIScriptContext> mScriptCX;
          if (NS_FAILED(nsLayoutUtils::GetStaticScriptContext(aContext, (JSObject*)GetDOMSlots()->mScriptObject, getter_AddRefs(mScriptCX))) ||
              NS_OK != manager->RegisterScriptEventListener(mScriptCX, this,
                                                            atom, kIDOMPaintListenerIID)) {
            NS_RELEASE(manager);
            return PR_FALSE;
          }
        }
      }
      NS_IF_RELEASE(manager);
    }
  }

  return PR_TRUE;
}

PRBool
nsGenericElement::EnumerateProperty(JSContext *aContext, JSObject *aObj)
{
  return PR_TRUE;
}

PRBool
nsGenericElement::Resolve(JSContext *aContext, JSObject *aObj, jsval aID,
                          PRBool* aDidDefineProperty)
{
  *aDidDefineProperty = PR_FALSE;

  return PR_TRUE;
}

PRBool
nsGenericElement::Convert(JSContext *aContext, JSObject *aObj, jsval aID)
{
  return PR_TRUE;
}

void
nsGenericElement::Finalize(JSContext *aContext, JSObject *aObj)
{
}

// Generic DOMNode implementations

/*
 * This helper function checks if aChild is the same as aNode or if
 * aChild is one of aNode's ancestors. -- jst@citec.fi
 */

static PRBool isSelfOrAncestor(nsIContent *aNode, nsIContent *aChild)
{
  if (aNode == aChild)
    return PR_TRUE;

  nsCOMPtr<nsIContent> parent, tmpNode;
  PRInt32 childCount = 0;

  /*
   * If aChild doesn't have children it can't be our ancestor
   */
  aChild->ChildCount(childCount);

  if (childCount <= 0) {
    return PR_FALSE;
  }

  aNode->GetParent(*getter_AddRefs(parent));

  while (parent) {
    if (parent.get() == aChild) {
      /*
       * We found aChild as one of our ancestors
       */
      return PR_TRUE;
    }

    parent->GetParent(*getter_AddRefs(tmpNode));

    parent = tmpNode;
  }

  return PR_FALSE;
}


nsresult
nsGenericElement::doInsertBefore(nsIDOMNode* aNewChild,
                                 nsIDOMNode* aRefChild,
                                 nsIDOMNode** aReturn)
{
  if (!aReturn) {
    return NS_ERROR_NULL_POINTER;
  }

  *aReturn = nsnull;

  if (!aNewChild) {
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<nsIContent> refContent;
  nsresult res = NS_OK;
  PRInt32 refPos = 0;

  if (aRefChild) {
    refContent = do_QueryInterface(aRefChild, &res);

    if (NS_FAILED(res)) {
      /*
       * If aRefChild doesn't support the nsIContent interface it can't be
       * an existing child of this node.
       */
      return NS_ERROR_DOM_NOT_FOUND_ERR;
    }

    IndexOf(refContent, refPos);

    if (refPos < 0) {
      return NS_ERROR_DOM_NOT_FOUND_ERR;
    }
  } else {
    ChildCount(refPos);
  }

  PRUint16 nodeType = 0;

  res = aNewChild->GetNodeType(&nodeType);

  if (NS_FAILED(res)) {
    return res;
  }

  switch (nodeType) {
  case nsIDOMNode::ELEMENT_NODE :
  case nsIDOMNode::TEXT_NODE :
  case nsIDOMNode::CDATA_SECTION_NODE :
  case nsIDOMNode::ENTITY_REFERENCE_NODE :
  case nsIDOMNode::PROCESSING_INSTRUCTION_NODE :
  case nsIDOMNode::COMMENT_NODE :
    break;
  case nsIDOMNode::DOCUMENT_FRAGMENT_NODE :
    break;
  default:
    /*
     * aNewChild is of invalid type.
     */
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  nsCOMPtr<nsIContent> newContent(do_QueryInterface(aNewChild, &res));

  if (NS_FAILED(res)) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  /*
   * Make sure the new child is not "this" node or one of this nodes
   * ancestors. Doing this check here should be safe even if newContent
   * is a document fragment.
   */
  if (isSelfOrAncestor(this, newContent)) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  /*
   * Check if this is a document fragment. If it is, we need
   * to remove the children of the document fragment and add them
   * individually (i.e. we don't add the actual document fragment).
   */
  if (nodeType == nsIDOMNode::DOCUMENT_FRAGMENT_NODE) {
    nsCOMPtr<nsIContent> childContent;
    PRInt32 count, i;

    newContent->ChildCount(count);

    /*
     * Iterate through the fragments children, removing each from
     * the fragment and inserting it into the child list of its
     * new parent.
     */
    for (i = 0; i < count; i++) {
      // Always get and remove the first child, since the child indexes
      // change as we go along.
      res = newContent->ChildAt(0, *getter_AddRefs(childContent));
      if (NS_FAILED(res)) {
        return res;
      }

      res = newContent->RemoveChildAt(0, PR_FALSE);

      if (NS_FAILED(res)) {
        return res;
      }

      childContent->SetDocument(mDocument, PR_TRUE, PR_TRUE);

      // Insert the child and increment the insertion position
      res = InsertChildAt(childContent, refPos++, PR_TRUE);

      if (NS_FAILED(res)) {
        return res;
      }
    }
  } else {
    nsCOMPtr<nsIDOMNode> oldParent;
    res = aNewChild->GetParentNode(getter_AddRefs(oldParent));

    if (NS_FAILED(res)) {
      return res;
    }

    /*
     * Remove the element from the old parent if one exists, since oldParent
     * is a nsIDOMNode this will do the right thing even if the parent of
     * aNewChild is a document. This code also handles the case where the
     * new child is alleady a child of this node-- jst@citec.fi
     */
    if (oldParent) {
      nsCOMPtr<nsIDOMNode> tmpNode;

      PRInt32 origChildCount, newChildCount;

      ChildCount(origChildCount);

      /*
       * We don't care here if the return fails or not.
       */
      oldParent->RemoveChild(aNewChild, getter_AddRefs(tmpNode));

      ChildCount(newChildCount);

      /*
       * Check if our child count changed during the RemoveChild call, if
       * it did then oldParent is most likely this node. In this case we
       * must check if refPos is still correct (unless it's zero).
       */
      if (refPos && origChildCount != newChildCount) {
        if (refContent) {
          /*
           * If we did get aRefChild we check if that is now at refPos - 1,
           * this will happend if the new child was one of aRefChilds'
           * previous siblings.
           */
          nsCOMPtr<nsIContent> tmpContent;

          ChildAt(refPos - 1, *getter_AddRefs(tmpContent));

          if (refContent == tmpContent) {
            refPos--;
          }
        } else {
          /*
           * If we didn't get aRefChild we simply decrement refPos.
           */
          refPos--;
        }
      }
    }

    newContent->SetDocument(mDocument, PR_TRUE, PR_TRUE);

    res = InsertChildAt(newContent, refPos, PR_TRUE);

    if (NS_FAILED(res)) {
      return res;
    }
  }

  *aReturn = aNewChild;
  NS_ADDREF(*aReturn);

  return res;
}


nsresult
nsGenericElement::doReplaceChild(nsIDOMNode* aNewChild,
                                 nsIDOMNode* aOldChild,
                                 nsIDOMNode** aReturn)
{
  if (!aReturn) {
    return NS_ERROR_NULL_POINTER;
  }

  *aReturn = nsnull;

  if (!aNewChild || !aOldChild) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult res = NS_OK;
  PRInt32 oldPos = 0;

  nsCOMPtr<nsIContent> oldContent(do_QueryInterface(aOldChild, &res));

  if (NS_FAILED(res)) {
    /*
     * If aOldChild doesn't support the nsIContent interface it can't be
     * an existing child of this node.
     */
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  IndexOf(oldContent, oldPos);

  if (oldPos < 0) {
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  nsCOMPtr<nsIContent> replacedChild;

  ChildAt(oldPos, *getter_AddRefs(replacedChild));

  PRUint16 nodeType = 0;

  res = aNewChild->GetNodeType(&nodeType);

  if (NS_FAILED(res)) {
    return res;
  }

  switch (nodeType) {
  case nsIDOMNode::ELEMENT_NODE :
  case nsIDOMNode::TEXT_NODE :
  case nsIDOMNode::CDATA_SECTION_NODE :
  case nsIDOMNode::ENTITY_REFERENCE_NODE :
  case nsIDOMNode::PROCESSING_INSTRUCTION_NODE :
  case nsIDOMNode::COMMENT_NODE :
  case nsIDOMNode::DOCUMENT_FRAGMENT_NODE :
    break;
  default:
    /*
     * aNewChild is of invalid type.
     */

    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  nsCOMPtr<nsIContent> newContent(do_QueryInterface(aNewChild, &res));

  if (NS_FAILED(res)) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  nsCOMPtr<nsIDocument> document;

  GetDocument(*getter_AddRefs(document));

  /*
   * Make sure the new child is not "this" node or one of this nodes
   * ancestors. Doing this check here should be safe even if newContent
   * is a document fragment.
   */
  if (isSelfOrAncestor(this, newContent)) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  /*
   * Check if this is a document fragment. If it is, we need
   * to remove the children of the document fragment and add them
   * individually (i.e. we don't add the actual document fragment).
   */
  if (nodeType == nsIDOMNode::DOCUMENT_FRAGMENT_NODE) {
    nsCOMPtr<nsIContent> childContent;
    PRInt32 count, i;

    newContent->ChildCount(count);

    /*
     * Iterate through the fragments children, removing each from
     * the fragment and inserting it into the child list of its
     * new parent.
     */
    for (i =0; i < count; i++) {
      // Always get and remove the first child, since the child indexes
      // change as we go along.
      res = newContent->ChildAt(0, *getter_AddRefs(childContent));
      if (NS_FAILED(res)) {
        return res;
      }

      res = newContent->RemoveChildAt(0, PR_FALSE);

      if (NS_FAILED(res)) {
        return res;
      }

      childContent->SetDocument(document, PR_TRUE, PR_TRUE);

      // Insert the child and increment the insertion position
      if (i) {
        res = InsertChildAt(childContent, oldPos++, PR_TRUE);
      } else {
        res = ReplaceChildAt(childContent, oldPos++, PR_TRUE);
      }

      if (NS_FAILED(res)) {
        return res;
      }
    }
  } else {
    nsCOMPtr<nsIDOMNode> oldParent;
    res = aNewChild->GetParentNode(getter_AddRefs(oldParent));

    if (NS_FAILED(res)) {
      return res;
    }

    /*
     * Remove the element from the old parent if one exists, since oldParent
     * is a nsIDOMNode this will do the right thing even if the parent of
     * aNewChild is a document. This code also handles the case where the
     * new child is alleady a child of this node-- jst@citec.fi
     */
    if (oldParent) {
      nsCOMPtr<nsIDOMNode> tmpNode;

      PRInt32 origChildCount, newChildCount;

      ChildCount(origChildCount);

      /*
       * We don't care here if the return fails or not.
       */
      oldParent->RemoveChild(aNewChild, getter_AddRefs(tmpNode));

      ChildCount(newChildCount);

      /*
       * Check if our child count changed during the RemoveChild call, if
       * it did then oldParent is most likely this node. In this case we
       * must check if oldPos is still correct (unless it's zero).
       */
      if (oldPos && origChildCount != newChildCount) {
        /*
         * Check if aOldChild is now at oldPos - 1, this will happend if
         * the new child was one of aOldChilds' previous siblings.
         */
        nsCOMPtr<nsIContent> tmpContent;

        ChildAt(oldPos - 1, *getter_AddRefs(tmpContent));

        if (oldContent == tmpContent) {
          oldPos--;
        }
      }
    }

    newContent->SetDocument(document, PR_TRUE, PR_TRUE);

    res = ReplaceChildAt(newContent, oldPos, PR_TRUE);

    if (NS_FAILED(res)) {
      return res;
    }
  }

  return replacedChild->QueryInterface(NS_GET_IID(nsIDOMNode),
                                       (void **)aReturn);
}


nsresult
nsGenericElement::doRemoveChild(nsIDOMNode* aOldChild,
                                nsIDOMNode** aReturn)
{
  *aReturn = nsnull;

  if (!aOldChild) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult res;

  nsCOMPtr<nsIContent> content(do_QueryInterface(aOldChild, &res));

  if (NS_FAILED(res)) {
    /*
     * If we're asked to remove something that doesn't support nsIContent
     * it can not be one of our children, i.e. we return NOT_FOUND_ERR.
     */
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  PRInt32 pos;
  IndexOf(content, pos);

  if (pos < 0) {
    /*
     * aOldChild isn't one of our children.
     */
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  res = RemoveChildAt(pos, PR_TRUE);

  *aReturn = aOldChild;
  NS_ADDREF(aOldChild);

  return res;
}

//----------------------------------------------------------------------

// nsISupports implementation

NS_IMETHODIMP
nsGenericElement::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  nsISupports *inst = nsnull;

  if (aIID.Equals(NS_GET_IID(nsISupports))) {
    inst = NS_STATIC_CAST(nsIContent *, this);
  } else if (aIID.Equals(NS_GET_IID(nsIContent))) {
    inst = NS_STATIC_CAST(nsIContent *, this);
  } else if (aIID.Equals(NS_GET_IID(nsIStyledContent))) {
    inst = NS_STATIC_CAST(nsIStyledContent *, this);
  } else if (aIID.Equals(NS_GET_IID(nsIDOMEventReceiver)) ||
             aIID.Equals(NS_GET_IID(nsIDOMEventTarget))) {
    nsCOMPtr<nsIEventListenerManager> man;

    GetListenerManager(getter_AddRefs(man));

    if (man) {
      return man->QueryInterface(aIID, aInstancePtr);
    }

    return NS_NOINTERFACE;
  } else if (aIID.Equals(NS_GET_IID(nsIScriptObjectOwner))) {
    inst = NS_STATIC_CAST(nsIScriptObjectOwner *, this);
  } else if (aIID.Equals(NS_GET_IID(nsIJSScriptObject))) {
    inst = NS_STATIC_CAST(nsIJSScriptObject *, this);
  } else {
    return NS_NOINTERFACE;
  }

  NS_ADDREF(inst);

  *aInstancePtr = inst;

  return NS_OK;
}

NS_IMPL_ADDREF(nsGenericElement)
NS_IMPL_RELEASE(nsGenericElement)

//----------------------------------------------------------------------

nsresult
nsGenericElement::TriggerLink(nsIPresContext* aPresContext,
                              nsLinkVerb aVerb,
                              nsIURI* aBaseURL,
                              const nsString& aURLSpec,
                              const nsString& aTargetSpec,
                              PRBool aClick)
{
  nsCOMPtr<nsILinkHandler> handler;
  nsresult rv = aPresContext->GetLinkHandler(getter_AddRefs(handler));
  if (NS_FAILED(rv) || (nsnull == handler)) return rv;

  // Resolve url to an absolute url
  nsAutoString absURLSpec;
  if (nsnull != aBaseURL) {
    rv = NS_MakeAbsoluteURI(absURLSpec, aURLSpec, aBaseURL);
    if (NS_FAILED(rv)) return rv;
  }
  else {
    absURLSpec.Assign(aURLSpec);
  }

  // Now pass on absolute url to the click handler
  if (aClick) {
    nsresult proceed = NS_OK;
    // Check that this page is allowed to load this URI.
    NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager,
                    NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    nsCOMPtr<nsIURI> absURI;
    if (NS_SUCCEEDED(rv))
      rv = NS_NewURI(getter_AddRefs(absURI), absURLSpec, aBaseURL);
    if (NS_SUCCEEDED(rv))
      proceed = securityManager->CheckLoadURI(aBaseURL, absURI, nsIScriptSecurityManager::STANDARD);

    // Only pass off the click event if the script security manager
    // says it's ok.
    if (NS_SUCCEEDED(proceed))
      handler->OnLinkClick(this, aVerb, absURLSpec.GetUnicode(),
                           aTargetSpec.GetUnicode());
  }
  else {
    handler->OnOverLink(this, absURLSpec.GetUnicode(),
                        aTargetSpec.GetUnicode());
  }
  return rv;
}

nsresult
nsGenericElement::AddScriptEventListener(nsIAtom* aAttribute,
                                         const nsAReadableString& aValue,
                                         REFNSIID aIID)
{
  nsresult ret = NS_OK;
  nsCOMPtr<nsIScriptContext> context = nsnull;
  nsCOMPtr<nsIScriptGlobalObject> global = nsnull;
  JSContext* cx = nsnull;

  //Try to get context from doc
  if (mDocument) {
    if (NS_SUCCEEDED(mDocument->GetScriptGlobalObject(getter_AddRefs(global))) && global) {
      NS_ENSURE_SUCCESS(global->GetContext(getter_AddRefs(context)), NS_ERROR_FAILURE);
    }
  }

  if (!context) {
    // Get JSContext from stack.
    nsCOMPtr<nsIThreadJSContextStack> stack(do_GetService("@mozilla.org/js/xpc/ContextStack;1"));
    NS_ENSURE_TRUE(stack, NS_ERROR_FAILURE);
    NS_ENSURE_SUCCESS(stack->Peek(&cx), NS_ERROR_FAILURE);

    if(!cx) {
      stack->GetSafeJSContext(&cx);
      NS_ENSURE_TRUE(cx, NS_ERROR_FAILURE);
    }

    nsLayoutUtils::GetDynamicScriptContext(cx, getter_AddRefs(context));
    NS_ENSURE_TRUE(context, NS_ERROR_FAILURE);
  }

  //Attributes on the body and frameset tags get set on the global object
  if (mNodeInfo->Equals(nsHTMLAtoms::body) ||
      mNodeInfo->Equals(nsHTMLAtoms::frameset)) {
    if (!global && cx) {
      nsLayoutUtils::GetDynamicScriptGlobal(cx, getter_AddRefs(global));

      NS_ENSURE_TRUE(global, NS_ERROR_FAILURE);
    }
    nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(global));
    NS_ENSURE_TRUE(receiver, NS_ERROR_FAILURE);

    nsCOMPtr<nsIEventListenerManager> manager;

    receiver->GetListenerManager(getter_AddRefs(manager));

    if (manager) {
      nsCOMPtr<nsIScriptObjectOwner> objOwner(do_QueryInterface(global));
      if (objOwner) {
        ret = manager->AddScriptEventListener(context, objOwner, aAttribute,
                                              aValue, aIID, PR_FALSE);
      }
    }
  }
  else {
    nsCOMPtr<nsIEventListenerManager> manager;
    GetListenerManager(getter_AddRefs(manager));

    if (manager) {
      ret = manager->AddScriptEventListener(context, this, aAttribute, aValue,
                                            aIID, PR_TRUE);
    }
  }

  return ret;
}


//----------------------------------------------------------------------

struct nsGenericAttribute
{
  nsGenericAttribute(nsINodeInfo *aNodeInfo, const nsAReadableString& aValue)
    : mNodeInfo(aNodeInfo),
      mValue(aValue)
  {
    NS_IF_ADDREF(mNodeInfo);
  }

  ~nsGenericAttribute(void)
  {
    NS_IF_RELEASE(mNodeInfo);
  }

#ifdef DEBUG
  nsresult SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const {
    if (!aResult) {
      return NS_ERROR_NULL_POINTER;
    }
    PRUint32 sum = sizeof(*this) - sizeof(mValue);
    PRUint32 ssize;
    mValue.SizeOf(aSizer, &ssize);
    sum += ssize;
    *aResult = sum;
    return NS_OK;
  }
#endif

  nsINodeInfo* mNodeInfo;
  nsString     mValue;
};

nsGenericContainerElement::nsGenericContainerElement()
{
  mAttributes = nsnull;
}

nsGenericContainerElement::~nsGenericContainerElement()
{
  PRInt32 count = mChildren.Count();
  PRInt32 index;
  for (index = 0; index < count; index++) {
    nsIContent* kid = (nsIContent *)mChildren.ElementAt(index);
    kid->SetParent(nsnull);
    NS_RELEASE(kid);
  }
  if (nsnull != mAttributes) {
    count = mAttributes->Count();
    for (index = 0; index < count; index++) {
      nsGenericAttribute* attr = (nsGenericAttribute*)mAttributes->ElementAt(index);
      delete attr;
    }
    delete mAttributes;
  }
}

nsresult
nsGenericContainerElement::NormalizeAttributeString(const nsAReadableString& aStr, nsINodeInfo*& aNodeInfo)
{
  if (mAttributes) {
    PRInt32 indx, count = mAttributes->Count();
    for (indx = 0; indx < count; indx++) {
      nsGenericAttribute* attr = (nsGenericAttribute*)mAttributes->ElementAt(indx);
      if (attr->mNodeInfo->QualifiedNameEquals(aStr)) {
        aNodeInfo = attr->mNodeInfo;
        NS_ADDREF(aNodeInfo);

        return NS_OK;
      }
    }
  }

  nsCOMPtr<nsINodeInfoManager> nimgr;
  mNodeInfo->GetNodeInfoManager(*getter_AddRefs(nimgr));
  NS_ENSURE_TRUE(nimgr, NS_ERROR_FAILURE);

  return nimgr->GetNodeInfo(aStr, nsnull, kNameSpaceID_None, aNodeInfo);
}

nsresult
nsGenericContainerElement::CopyInnerTo(nsIContent* aSrcContent,
                                       nsGenericContainerElement* aDst,
                                       PRBool aDeep)
{
  nsresult result = NS_OK;

  if (nsnull != mAttributes) {
    nsGenericAttribute* attr;
    PRInt32 index;
    PRInt32 count = mAttributes->Count();
    for (index = 0; index < count; index++) {
      attr = (nsGenericAttribute*)mAttributes->ElementAt(index);
      // XXX Not very efficient, since SetAttribute does a linear search
      // through its attributes before setting each attribute.
      result = aDst->SetAttribute(attr->mNodeInfo, attr->mValue, PR_FALSE);
      if (NS_OK != result) {
        return result;
      }
    }
  }

  if (aDeep) {
    PRInt32 index;
    PRInt32 count = mChildren.Count();
    for (index = 0; index < count; index++) {
      nsIContent* child = (nsIContent*)mChildren.ElementAt(index);
      if (nsnull != child) {
        nsIDOMNode* node;
        result = child->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)&node);
        if (NS_OK == result) {
          nsIDOMNode* newNode;

          result = node->CloneNode(aDeep, &newNode);
          if (NS_OK == result) {
            nsIContent* newContent;

            result = newNode->QueryInterface(NS_GET_IID(nsIContent),
                                             (void**)&newContent);
            if (NS_OK == result) {
              result = aDst->AppendChildTo(newContent, PR_FALSE);
              NS_RELEASE(newContent);
            }
            NS_RELEASE(newNode);
          }
          NS_RELEASE(node);
        }

        if (NS_OK != result) {
          return result;
        }
      }
    }
  }

  return result;
}

nsresult
nsGenericContainerElement::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  nsDOMSlots *slots = GetDOMSlots();

  if (nsnull == slots->mChildNodes) {
    slots->mChildNodes = new nsChildContentList(this);
    if (nsnull == slots->mChildNodes) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(slots->mChildNodes);
  }

  return slots->mChildNodes->QueryInterface(NS_GET_IID(nsIDOMNodeList),
                                            (void **)aChildNodes);
}

nsresult
nsGenericContainerElement::HasChildNodes(PRBool* aReturn)
{
  if (0 != mChildren.Count()) {
    *aReturn = PR_TRUE;
  }
  else {
    *aReturn = PR_FALSE;
  }
  return NS_OK;
}

nsresult
nsGenericContainerElement::GetFirstChild(nsIDOMNode** aNode)
{
  nsIContent *child = (nsIContent *)mChildren.ElementAt(0);
  if (nsnull != child) {
    nsresult res = child->QueryInterface(NS_GET_IID(nsIDOMNode),
                                         (void**)aNode);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Node"); // must be a DOM Node
    return res;
  }
  *aNode = nsnull;
  return NS_OK;
}

nsresult
nsGenericContainerElement::GetLastChild(nsIDOMNode** aNode)
{
  nsIContent *child = (nsIContent *)mChildren.ElementAt(mChildren.Count()-1);
  if (nsnull != child) {
    nsresult res = child->QueryInterface(NS_GET_IID(nsIDOMNode),
                                         (void**)aNode);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Node"); // must be a DOM Node
    return res;
  }
  *aNode = nsnull;
  return NS_OK;
}

nsresult
nsGenericContainerElement::SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                                        const nsAReadableString& aValue,
                                        PRBool aNotify)
{
  nsresult rv;
  nsCOMPtr<nsINodeInfoManager> nimgr;
  rv = mNodeInfo->GetNodeInfoManager(*getter_AddRefs(nimgr));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsINodeInfo> ni;
  rv = nimgr->GetNodeInfo(aName, nsnull, aNameSpaceID,
                          *getter_AddRefs(ni));
  NS_ENSURE_SUCCESS(rv, rv);

  return SetAttribute(ni, aValue, aNotify);
}

// Static helper method

PRBool nsGenericElement::HasMutationListeners(nsIContent* aContent,
                                              PRUint32 aType)
{
  nsCOMPtr<nsIDocument> doc;
  aContent->GetDocument(*getter_AddRefs(doc));
  if (!doc)
    return PR_FALSE;

  nsCOMPtr<nsIScriptGlobalObject> global;
  doc->GetScriptGlobalObject(getter_AddRefs(global));
  if (!global)
    return PR_FALSE;

  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(global));
  if (!window)
    return PR_FALSE;

  PRBool set;
  window->HasMutationListeners(aType, &set);
  if (!set)
    return PR_FALSE;

  // We know a mutation listener is registered, but it might not
  // be in our chain.  Check quickly to see.
  nsCOMPtr<nsIContent> curr = aContent;
  nsCOMPtr<nsIEventListenerManager> manager;

  while (curr) {
    nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(curr));
    if (rec) {
      rec->GetListenerManager(getter_AddRefs(manager));
      if (manager) {
        PRBool hasMutationListeners = PR_FALSE;
        manager->HasMutationListeners(&hasMutationListeners);
        if (hasMutationListeners)
          return PR_TRUE;
      }
    }

    nsCOMPtr<nsIContent> prev = curr;
    prev->GetParent(*getter_AddRefs(curr));
  }

  nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(doc));
  if (rec) {
    rec->GetListenerManager(getter_AddRefs(manager));
    if (manager) {
      PRBool hasMutationListeners = PR_FALSE;
      manager->HasMutationListeners(&hasMutationListeners);
      if (hasMutationListeners)
        return PR_TRUE;
    }
  }

  rec = do_QueryInterface(window);
  if (rec) {
    rec->GetListenerManager(getter_AddRefs(manager));
    if (manager) {
      PRBool hasMutationListeners = PR_FALSE;
      manager->HasMutationListeners(&hasMutationListeners);
      if (hasMutationListeners)
        return PR_TRUE;
    }
  }

  return PR_FALSE;
}

nsresult
nsGenericContainerElement::SetAttribute(nsINodeInfo* aNodeInfo,
                                        const nsAReadableString& aValue,
                                        PRBool aNotify)
{
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  PRBool modification = PR_FALSE;
  nsAutoString oldValue;

  nsresult rv = NS_ERROR_OUT_OF_MEMORY;

  if (!mAttributes) {
    mAttributes = new nsVoidArray();
    NS_ENSURE_TRUE(mAttributes, NS_ERROR_OUT_OF_MEMORY);
  }

  if (aNotify && (nsnull != mDocument)) {
    mDocument->BeginUpdate();
  }

  nsGenericAttribute* attr;
  PRInt32 index;
  PRInt32 count = mAttributes->Count();
  for (index = 0; index < count; index++) {
    attr = (nsGenericAttribute*)mAttributes->ElementAt(index);
    if (attr->mNodeInfo == aNodeInfo) {
      oldValue = attr->mValue;
      modification = PR_TRUE;
      attr->mValue = aValue;
      rv = NS_OK;
      break;
    }
  }

  if (index >= count) { // didn't find it
    attr = new nsGenericAttribute(aNodeInfo, aValue);
    NS_ENSURE_TRUE(attr, NS_ERROR_OUT_OF_MEMORY);

    mAttributes->AppendElement(attr);
    rv = NS_OK;
  }

  if (mDocument && NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIAtom> name;
    PRInt32 nameSpaceID;

    aNodeInfo->GetNameAtom(*getter_AddRefs(name));
    aNodeInfo->GetNamespaceID(nameSpaceID);

    nsCOMPtr<nsIBindingManager> bindingManager;
    mDocument->GetBindingManager(getter_AddRefs(bindingManager));
    nsCOMPtr<nsIXBLBinding> binding;
    bindingManager->GetBinding(this, getter_AddRefs(binding));
    if (binding)
      binding->AttributeChanged(name, nameSpaceID, PR_FALSE);

    if (HasMutationListeners(this, NS_EVENT_BITS_MUTATION_ATTRMODIFIED)) {
      nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(NS_STATIC_CAST(nsIContent *, this)));
      nsMutationEvent mutation;
      mutation.eventStructType = NS_MUTATION_EVENT;
      mutation.message = NS_MUTATION_ATTRMODIFIED;
      mutation.mTarget = node;

      nsAutoString attrName;
      name->ToString(attrName);
      nsCOMPtr<nsIDOMAttr> attrNode;
      GetAttributeNode(attrName, getter_AddRefs(attrNode));
      mutation.mRelatedNode = attrNode;

      mutation.mAttrName = name;
      if (!oldValue.IsEmpty())
        mutation.mPrevAttrValue = getter_AddRefs(NS_NewAtom(oldValue));
      if (!aValue.IsEmpty())
        mutation.mNewAttrValue = getter_AddRefs(NS_NewAtom(aValue));
      mutation.mAttrChange = modification ? nsIDOMMutationEvent::MODIFICATION :
                                             nsIDOMMutationEvent::ADDITION;
      nsEventStatus status = nsEventStatus_eIgnore;
      nsCOMPtr<nsIDOMEvent> domEvent;
      HandleDOMEvent(nsnull, &mutation, getter_AddRefs(domEvent),
                     NS_EVENT_FLAG_INIT, &status);
    }

    if (aNotify) {
      mDocument->AttributeChanged(this, nameSpaceID, name,
                                  NS_STYLE_HINT_UNKNOWN);
      mDocument->EndUpdate();
    }
  }

  return rv;
}

nsresult
nsGenericContainerElement::GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                                        nsAWritableString& aResult) const
{
  nsCOMPtr<nsIAtom> prefix;
  return GetAttribute(aNameSpaceID, aName, *getter_AddRefs(prefix), aResult);
}

nsresult
nsGenericContainerElement::GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                                        nsIAtom*& aPrefix,
                                        nsAWritableString& aResult) const
{
  NS_ASSERTION(nsnull != aName, "must have attribute name");
  if (nsnull == aName) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_CONTENT_ATTR_NOT_THERE;

  if (nsnull != mAttributes) {
    PRInt32 count = mAttributes->Count();
    PRInt32 index;
    for (index = 0; index < count; index++) {
      const nsGenericAttribute* attr = (const nsGenericAttribute*)mAttributes->ElementAt(index);
      if ((aNameSpaceID == kNameSpaceID_Unknown ||
           attr->mNodeInfo->NamespaceEquals(aNameSpaceID)) &&
          (attr->mNodeInfo->Equals(aName))) {
        attr->mNodeInfo->GetPrefixAtom(aPrefix);
        aResult.Assign(attr->mValue);
        if (0 < aResult.Length()) {
          rv = NS_CONTENT_ATTR_HAS_VALUE;
        }
        else {
          rv = NS_CONTENT_ATTR_NO_VALUE;
        }
        break;
      }
    }
  }

  if (rv == NS_CONTENT_ATTR_NOT_THERE) {
    // In other cases we already set the out param.
    // Since we are returning a success code we'd better do
    // something about the out parameters (someone may have
    // given us a non-empty string).
    aResult.Truncate();
  }

  return rv;
}

nsresult
nsGenericContainerElement::UnsetAttribute(PRInt32 aNameSpaceID,
                                          nsIAtom* aName, PRBool aNotify)
{
  NS_ASSERTION(nsnull != aName, "must have attribute name");
  if (nsnull == aName) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_OK;

  if (nsnull != mAttributes) {
    PRInt32 count = mAttributes->Count();
    PRInt32 index;
    PRBool  found = PR_FALSE;
    for (index = 0; index < count; index++) {
      nsGenericAttribute* attr = (nsGenericAttribute*)mAttributes->ElementAt(index);
      if ((aNameSpaceID == kNameSpaceID_Unknown ||
           attr->mNodeInfo->NamespaceEquals(aNameSpaceID)) &&
          attr->mNodeInfo->Equals(aName)) {
        if (aNotify && (nsnull != mDocument)) {
          mDocument->BeginUpdate();
        }

        if (HasMutationListeners(this, NS_EVENT_BITS_MUTATION_ATTRMODIFIED)) {
          nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(NS_STATIC_CAST(nsIContent *, this)));
          nsMutationEvent mutation;
          mutation.eventStructType = NS_MUTATION_EVENT;
          mutation.message = NS_MUTATION_ATTRMODIFIED;
          mutation.mTarget = node;

          nsAutoString attrName;
          aName->ToString(attrName);
          nsCOMPtr<nsIDOMAttr> attrNode;
          GetAttributeNode(attrName, getter_AddRefs(attrNode));
          mutation.mRelatedNode = attrNode;

          mutation.mAttrName = aName;
          if (!attr->mValue.IsEmpty())
            mutation.mPrevAttrValue = getter_AddRefs(NS_NewAtom(attr->mValue));
          mutation.mAttrChange = nsIDOMMutationEvent::REMOVAL;

          nsEventStatus status = nsEventStatus_eIgnore;
          nsCOMPtr<nsIDOMEvent> domEvent;
          this->HandleDOMEvent(nsnull, &mutation, getter_AddRefs(domEvent),
                               NS_EVENT_FLAG_INIT, &status);
        }

        mAttributes->RemoveElementAt(index);
        delete attr;
        found = PR_TRUE;
        break;
      }
    }

    if (NS_SUCCEEDED(rv) && found && mDocument) {
      nsCOMPtr<nsIBindingManager> bindingManager;
      mDocument->GetBindingManager(getter_AddRefs(bindingManager));
      nsCOMPtr<nsIXBLBinding> binding;
      bindingManager->GetBinding(this, getter_AddRefs(binding));
      if (binding)
        binding->AttributeChanged(aName, aNameSpaceID, PR_TRUE);

      if (aNotify) {
        mDocument->AttributeChanged(this, aNameSpaceID, aName,
                                    NS_STYLE_HINT_UNKNOWN);
        mDocument->EndUpdate();
      }
    }
  }

  return rv;
}

nsresult
nsGenericContainerElement::GetAttributeNameAt(PRInt32 aIndex,
                                              PRInt32& aNameSpaceID,
                                              nsIAtom*& aName,
                                              nsIAtom*& aPrefix) const
{
  if (nsnull != mAttributes) {
    nsGenericAttribute* attr = (nsGenericAttribute*)mAttributes->ElementAt(aIndex);
    if (nsnull != attr) {
      attr->mNodeInfo->GetNamespaceID(aNameSpaceID);
      attr->mNodeInfo->GetNameAtom(aName);
      attr->mNodeInfo->GetPrefixAtom(aPrefix);

      return NS_OK;
    }
  }
  aNameSpaceID = kNameSpaceID_None;
  aName = nsnull;
  return NS_ERROR_ILLEGAL_VALUE;
}

nsresult
nsGenericContainerElement::GetAttributeCount(PRInt32& aResult) const
{
  if (nsnull != mAttributes) {
    aResult = mAttributes->Count();
  }
  else {
    aResult = 0;
  }
  return NS_OK;
}

void
nsGenericContainerElement::ListAttributes(FILE* out) const
{
  PRInt32 index, count;
  GetAttributeCount(count);

  for (index = 0; index < count; index++) {
    const nsGenericAttribute* attr = (const nsGenericAttribute*)mAttributes->ElementAt(index);
    nsAutoString buffer;

    // name
    nsAutoString name;
    attr->mNodeInfo->GetQualifiedName(name);
    buffer.Append(name);

    // value
    buffer.AppendWithConversion("=");
    buffer.Append(attr->mValue);

    fputs(" ", out);
    fputs(buffer, out);
  }
}

nsresult
nsGenericContainerElement::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(nsnull != mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString buf;
  mNodeInfo->GetQualifiedName(buf);
  fputs(buf, out);

  fprintf(out, "@%p", this);

  ListAttributes(out);

  fprintf(out, " refcount=%d<", mRefCnt);

  PRBool canHaveKids;
  CanContainChildren(canHaveKids);
  if (canHaveKids) {
    fputs("\n", out);
    PRInt32 kids;
    ChildCount(kids);
    for (index = 0; index < kids; index++) {
      nsIContent* kid;
      ChildAt(index, kid);
      kid->List(out, aIndent + 1);
      NS_RELEASE(kid);
    }
    for (index = aIndent; --index >= 0; ) fputs("  ", out);
  }
  fputs(">\n", out);

  return NS_OK;
}

nsresult
nsGenericContainerElement::DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const {
  return NS_OK;
}

nsresult
nsGenericContainerElement::CanContainChildren(PRBool& aResult) const
{
  aResult = PR_TRUE;
  return NS_OK;
}

nsresult
nsGenericContainerElement::ChildCount(PRInt32& aCount) const
{
  aCount = mChildren.Count();
  return NS_OK;
}

nsresult
nsGenericContainerElement::ChildAt(PRInt32 aIndex,
                                   nsIContent*& aResult) const
{
  nsIContent *child = (nsIContent *)mChildren.ElementAt(aIndex);
  if (nsnull != child) {
    NS_ADDREF(child);
  }
  aResult = child;
  return NS_OK;
}

nsresult
nsGenericContainerElement::IndexOf(nsIContent* aPossibleChild,
                                   PRInt32& aIndex) const
{
  NS_PRECONDITION(nsnull != aPossibleChild, "null ptr");
  aIndex = mChildren.IndexOf(aPossibleChild);
  return NS_OK;
}

nsresult
nsGenericContainerElement::InsertChildAt(nsIContent* aKid,
                                         PRInt32 aIndex,
                                         PRBool aNotify)
{
  NS_PRECONDITION(nsnull != aKid, "null ptr");
  nsIDocument* doc = mDocument;
  if (aNotify && (nsnull != doc)) {
    doc->BeginUpdate();
  }
  PRBool rv = mChildren.InsertElementAt(aKid, aIndex);/* XXX fix up void array api to use nsresult's*/
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(this);
    nsRange::OwnerChildInserted(this, aIndex);
    if (nsnull != doc) {
      aKid->SetDocument(doc, PR_FALSE, PR_TRUE);
      if (aNotify) {
        doc->ContentInserted(this, aKid, aIndex);
      }

      if (HasMutationListeners(this, NS_EVENT_BITS_MUTATION_NODEINSERTED)) {
        nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(aKid));
        nsMutationEvent mutation;
        mutation.eventStructType = NS_MUTATION_EVENT;
        mutation.message = NS_MUTATION_NODEINSERTED;
        mutation.mTarget = node;

        nsCOMPtr<nsIDOMNode> relNode(do_QueryInterface(NS_STATIC_CAST(nsIContent *, this)));
        mutation.mRelatedNode = relNode;

        nsEventStatus status = nsEventStatus_eIgnore;
        nsCOMPtr<nsIDOMEvent> domEvent;
        aKid->HandleDOMEvent(nsnull, &mutation, getter_AddRefs(domEvent),
                             NS_EVENT_FLAG_INIT, &status);
      }
    }
  }
  if (aNotify && (nsnull != doc)) {
    doc->EndUpdate();
  }
  return NS_OK;
}

nsresult
nsGenericContainerElement::ReplaceChildAt(nsIContent* aKid,
                                          PRInt32 aIndex,
                                          PRBool aNotify)
{
  NS_PRECONDITION(nsnull != aKid, "null ptr");
  nsIDocument* doc = mDocument;
  if (aNotify && (nsnull != mDocument)) {
    doc->BeginUpdate();
  }
  nsIContent* oldKid = (nsIContent *)mChildren.ElementAt(aIndex);
  nsRange::OwnerChildReplaced(this, aIndex, oldKid);
  PRBool rv = mChildren.ReplaceElementAt(aKid, aIndex);
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(this);
    if (nsnull != doc) {
      aKid->SetDocument(doc, PR_FALSE, PR_TRUE);
      if (aNotify) {
        doc->ContentReplaced(this, oldKid, aKid, aIndex);
      }
    }
    oldKid->SetDocument(nsnull, PR_TRUE, PR_TRUE);
    oldKid->SetParent(nsnull);
    NS_RELEASE(oldKid);
  }
  if (aNotify && (nsnull != mDocument)) {
    doc->EndUpdate();
  }
  return NS_OK;
}

nsresult
nsGenericContainerElement::AppendChildTo(nsIContent* aKid, PRBool aNotify)
{
  NS_PRECONDITION(nsnull != aKid, "null ptr");
  nsIDocument* doc = mDocument;
  if (aNotify && (nsnull != doc)) {
    doc->BeginUpdate();
  }
  PRBool rv = mChildren.AppendElement(aKid);
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(this);
    // ranges don't need adjustment since new child is at end of list
    if (nsnull != doc) {
      aKid->SetDocument(doc, PR_FALSE, PR_TRUE);
      if (aNotify) {
        doc->ContentAppended(this, mChildren.Count() - 1);
      }

      if (HasMutationListeners(this, NS_EVENT_BITS_MUTATION_NODEINSERTED)) {
        nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(aKid));
        nsMutationEvent mutation;
        mutation.eventStructType = NS_MUTATION_EVENT;
        mutation.message = NS_MUTATION_NODEINSERTED;
        mutation.mTarget = node;

        nsCOMPtr<nsIDOMNode> relNode(do_QueryInterface(NS_STATIC_CAST(nsIContent *, this)));
        mutation.mRelatedNode = relNode;

        nsEventStatus status = nsEventStatus_eIgnore;
        nsCOMPtr<nsIDOMEvent> domEvent;
        aKid->HandleDOMEvent(nsnull, &mutation, getter_AddRefs(domEvent),
                             NS_EVENT_FLAG_INIT, &status);
      }
    }
  }
  if (aNotify && (nsnull != doc)) {
    doc->EndUpdate();
  }
  return NS_OK;
}

nsresult
nsGenericContainerElement::RemoveChildAt(PRInt32 aIndex, PRBool aNotify)
{
  nsIContent* oldKid = (nsIContent *)mChildren.ElementAt(aIndex);
  if (nsnull != oldKid ) {
    nsIDocument* doc = mDocument;
    if (aNotify && (nsnull != doc)) {
      doc->BeginUpdate();
    }

    if (HasMutationListeners(this, NS_EVENT_BITS_MUTATION_NODEREMOVED)) {
      nsCOMPtr<nsIDOMEventTarget> node(do_QueryInterface(oldKid));
      nsMutationEvent mutation;
      mutation.eventStructType = NS_MUTATION_EVENT;
      mutation.message = NS_MUTATION_NODEREMOVED;
      mutation.mTarget = node;

      nsCOMPtr<nsIDOMNode> relNode(do_QueryInterface(NS_STATIC_CAST(nsIContent *, this)));
      mutation.mRelatedNode = relNode;

      nsEventStatus status = nsEventStatus_eIgnore;
      nsCOMPtr<nsIDOMEvent> domEvent;
      oldKid->HandleDOMEvent(nsnull, &mutation, getter_AddRefs(domEvent),
                             NS_EVENT_FLAG_INIT, &status);
    }

    nsRange::OwnerChildRemoved(this, aIndex, oldKid);

    mChildren.RemoveElementAt(aIndex);
    if (aNotify) {
      if (nsnull != doc) {
        doc->ContentRemoved(this, oldKid, aIndex);
      }
    }
    oldKid->SetDocument(nsnull, PR_TRUE, PR_TRUE);
    oldKid->SetParent(nsnull);
    NS_RELEASE(oldKid);
    if (aNotify && (nsnull != doc)) {
      doc->EndUpdate();
    }
  }

  return NS_OK;
}

PRUint32
nsGenericContainerElement::BaseSizeOf(nsISizeOfHandler *aSizer) const
{
  PRUint32 sum = 0;
#ifdef DEBUG
  if (mAttributes) {
    // Add in array of attributes size
    PRUint32 asize;
    mAttributes->SizeOf(aSizer, &asize);
    sum += asize;

    // Add in each attributes size
    PRInt32 i, n = mAttributes->Count();
    for (i = 0; i < n; i++) {
      const nsGenericAttribute* attr = (const nsGenericAttribute*)
        mAttributes->ElementAt(i);
      if (attr) {
        PRUint32 asum = 0;
        attr->SizeOf(aSizer, &asum);
        sum += asum;
      }
    }
  }
#endif

  return sum;
}

