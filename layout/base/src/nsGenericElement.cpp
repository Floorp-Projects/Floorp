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
#include "nsIDocument.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMRange.h"
#include "nsIDOMText.h"
#include "nsRange.h"
#include "nsIEventListenerManager.h"
#include "nsILinkHandler.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectOwner.h"
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
#include "prprf.h"
#include "prmem.h"
#include "nsDOMError.h"
#include "nsScriptSecurityManager.h"

#include "nsLayoutAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsLayoutUtils.h"

NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);
NS_DEFINE_IID(kIDOMEventReceiverIID, NS_IDOMEVENTRECEIVER_IID);
NS_DEFINE_IID(kIDOMEventTargetIID, NS_IDOMEVENTTARGET_IID);
NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);

static NS_DEFINE_IID(kIDOMAttrIID, NS_IDOMATTR_IID);
static NS_DEFINE_IID(kIDOMNamedNodeMapIID, NS_IDOMNAMEDNODEMAP_IID);
static NS_DEFINE_IID(kIPrivateDOMEventIID, NS_IPRIVATEDOMEVENT_IID);
static NS_DEFINE_IID(kIDOMNodeListIID, NS_IDOMNODELIST_IID);
static NS_DEFINE_IID(kIDOMCSSStyleDeclarationIID, NS_IDOMCSSSTYLEDECLARATION_IID);
static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMDocumentFragmentIID, NS_IDOMDOCUMENTFRAGMENT_IID);

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
      res = content->QueryInterface(kIDOMNodeIID, (void**)aReturn);
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

static NS_DEFINE_IID(kIDOMScriptObjectFactoryIID, NS_IDOM_SCRIPT_OBJECT_FACTORY_IID);
static NS_DEFINE_IID(kDOMScriptObjectFactoryCID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

nsresult 
nsGenericElement::GetScriptObjectFactory(nsIDOMScriptObjectFactory **aResult)
{
  nsresult result = NS_OK;

  if (nsnull == gScriptObjectFactory) {
    result = nsServiceManager::GetService(kDOMScriptObjectFactoryCID,
                                          kIDOMScriptObjectFactoryIID,
                                          (nsISupports **)&gScriptObjectFactory);
    if (result != NS_OK) {
      return result;
    }
  }

  *aResult = gScriptObjectFactory;
  NS_ADDREF(gScriptObjectFactory);
  return result;
}

nsGenericElement::nsGenericElement()
{
  mDocument = nsnull;
  mParent = nsnull;
  mTag = nsnull;
  mContent = nsnull;
  mDOMSlots = nsnull;
  mContentID = 0;
}

nsGenericElement::~nsGenericElement()
{
  // pop any enclosed ranges out
  // nsRange::OwnerGone(mContent); not used for now
  NS_IF_RELEASE(mTag);
  if (nsnull != mDOMSlots) {
    if (nsnull != mDOMSlots->mChildNodes) {
      mDOMSlots->mChildNodes->DropReference();
      NS_RELEASE(mDOMSlots->mChildNodes);
      delete mDOMSlots->mRangeList;
    }
    if (nsnull != mDOMSlots->mStyle) {
      mDOMSlots->mStyle->DropReference();
      NS_RELEASE(mDOMSlots->mStyle);
    } 
    if (nsnull != mDOMSlots->mAttributeMap) {
      mDOMSlots->mAttributeMap->DropReference();
      NS_RELEASE(mDOMSlots->mAttributeMap);
    }
    NS_IF_RELEASE(mDOMSlots->mListenerManager);
    // XXX Should really be arena managed
    PR_DELETE(mDOMSlots);
  }
}

nsDOMSlots *
nsGenericElement::GetDOMSlots()
{
  if (nsnull == mDOMSlots) {
    mDOMSlots = PR_NEW(nsDOMSlots);
    mDOMSlots->mScriptObject = nsnull;
    mDOMSlots->mChildNodes = nsnull;
    mDOMSlots->mStyle = nsnull;
    mDOMSlots->mAttributeMap = nsnull;
    mDOMSlots->mRangeList = nsnull;
    mDOMSlots->mCapturer = nsnull;
    mDOMSlots->mListenerManager = nsnull;
  }
  
  return mDOMSlots;
}

void
nsGenericElement::MaybeClearDOMSlots()
{
  if (mDOMSlots &&
      (nsnull == mDOMSlots->mScriptObject) &&
      (nsnull == mDOMSlots->mChildNodes) &&
      (nsnull == mDOMSlots->mStyle) &&
      (nsnull == mDOMSlots->mAttributeMap) &&
      (nsnull == mDOMSlots->mRangeList) &&
      (nsnull == mDOMSlots->mCapturer) &&
      (nsnull == mDOMSlots->mListenerManager)) {
    PR_DELETE(mDOMSlots);
  }
}

void
nsGenericElement::Init(nsIContent* aOuterContentObject,
                       nsIAtom* aTag)
{
  NS_ASSERTION((nsnull == mContent) && (nsnull != aOuterContentObject),
               "null ptr");
  mContent = aOuterContentObject;
  mTag = aTag;
  NS_IF_ADDREF(aTag);
}

nsresult
nsGenericElement::GetNodeName(nsString& aNodeName)
{
  return GetTagName(aNodeName);
}

nsresult
nsGenericElement::GetLocalName(nsString& aNodeName)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsGenericElement::GetNodeValue(nsString& aNodeValue)
{
  aNodeValue.Truncate();
  return NS_OK;
}

nsresult
nsGenericElement::SetNodeValue(const nsString& aNodeValue)
{
  // The node value can't be modified
  return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
}

nsresult
nsGenericElement::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::ELEMENT_NODE;
  return NS_OK;
}

nsresult
nsGenericElement::GetParentNode(nsIDOMNode** aParentNode)
{
  nsresult res = NS_OK;

  if (nsnull != mParent) {
    res = mParent->QueryInterface(kIDOMNodeIID, (void**)aParentNode);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Node");
  }
  else if (nsnull == mDocument) {
    *aParentNode = nsnull;
  } 
  else {
    // If we don't have a parent, but we're in the document, we must
    // be the root node of the document. The DOM says that the root
    // is the document.
    res = mDocument->QueryInterface(kIDOMNodeIID, (void**)aParentNode);
  }

  return res;
}

nsresult
nsGenericElement::GetPreviousSibling(nsIDOMNode** aPrevSibling)
{
  nsIContent* sibling = nsnull;
  nsresult result = NS_OK;

  if (nsnull != mParent) {
    PRInt32 pos;
    mParent->IndexOf(mContent, pos);
    if (pos > -1 ) {
      mParent->ChildAt(--pos, sibling);
    }
  }
  else if (nsnull != mDocument) {
    // Nodes that are just below the document (their parent is the
    // document) need to go to the document to find their next sibling.
    PRInt32 pos;
    mDocument->IndexOf(mContent, pos);
    if (pos > -1 ) {
      mDocument->ChildAt(--pos, sibling);
    }    
  }

  if (nsnull != sibling) {
    result = sibling->QueryInterface(kIDOMNodeIID,(void**)aPrevSibling);
    NS_ASSERTION(NS_OK == result, "Must be a DOM Node");
    NS_RELEASE(sibling); // balance the AddRef in ChildAt()
  }
  else {
    *aPrevSibling = nsnull;
  }
  
  return result;
}

nsresult
nsGenericElement::GetNextSibling(nsIDOMNode** aNextSibling)
{
  nsIContent* sibling = nsnull;
  nsresult result = NS_OK;

  if (nsnull != mParent) {
    PRInt32 pos;
    mParent->IndexOf(mContent, pos);
    if (pos > -1 ) {
      mParent->ChildAt(++pos, sibling);
    }
  }
  else if (nsnull != mDocument) {
    // Nodes that are just below the document (their parent is the
    // document) need to go to the document to find their next sibling.
    PRInt32 pos;
    mDocument->IndexOf(mContent, pos);
    if (pos > -1 ) {
      mDocument->ChildAt(++pos, sibling);
    }    
  }

  if (nsnull != sibling) {
    result = sibling->QueryInterface(kIDOMNodeIID,(void**)aNextSibling);
    NS_ASSERTION(NS_OK == result, "Must be a DOM Node");
    NS_RELEASE(sibling); // balance the AddRef in ChildAt()
  }
  else {
    *aNextSibling = nsnull;
  }
  
  return result;
}

nsresult
nsGenericElement::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  // XXX Actually the owner document is the document in whose context
  // the element has been created. We should be able to get at it
  // whether or not we are attached to the document.
  if (nsnull != mDocument) {
    return mDocument->QueryInterface(kIDOMDocumentIID, (void **)aOwnerDocument);
  }
  else {
    *aOwnerDocument = nsnull;
    return NS_OK;
  }
}

nsresult
nsGenericElement::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
  NS_PRECONDITION(nsnull != aAttributes, "null pointer argument");
  nsDOMSlots *slots = GetDOMSlots();

  if (nsnull == slots->mAttributeMap) {
    slots->mAttributeMap = new nsDOMAttributeMap(mContent);
    if (nsnull == slots->mAttributeMap) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(slots->mAttributeMap);
  }

  return slots->mAttributeMap->QueryInterface(kIDOMNamedNodeMapIID, 
                                              (void **)aAttributes);
}

nsresult
nsGenericElement::GetTagName(nsString& aTagName)
{
  aTagName.Truncate();
  if (nsnull != mTag) {
    // note that we assume no namespace here, subclasses that support
    // namespaces must override
    mTag->ToString(aTagName);
  }
  return NS_OK;
}

nsresult
nsGenericElement::GetAttribute(const nsString& aName, nsString& aReturn)
{
  nsIAtom* nameAtom;
  PRInt32 nameSpaceID;

  mContent->ParseAttributeString(aName, nameAtom, nameSpaceID);
  if (kNameSpaceID_Unknown == nameSpaceID) {
    nameSpaceID = kNameSpaceID_None;  // ignore unknown prefix XXX is this correct?
  }
  mContent->GetAttribute(nameSpaceID, nameAtom, aReturn);
  NS_RELEASE(nameAtom);

  return NS_OK;
}

nsresult
nsGenericElement::SetAttribute(const nsString& aName,
                               const nsString& aValue)
{
  nsIAtom* nameAtom;
  PRInt32 nameSpaceID;
  nsresult result = NS_OK;

  mContent->ParseAttributeString(aName, nameAtom, nameSpaceID);
  if (kNameSpaceID_Unknown == nameSpaceID) {
    nameSpaceID = kNameSpaceID_None;  // ignore unknown prefix XXX is this correct?
  }
  result = mContent->SetAttribute(nameSpaceID, nameAtom, aValue, PR_TRUE);
  NS_RELEASE(nameAtom);

  return result;
}

nsresult
nsGenericElement::RemoveAttribute(const nsString& aName)
{
  nsIAtom* nameAtom;
  PRInt32 nameSpaceID;
  nsresult result = NS_OK;

  mContent->ParseAttributeString(aName, nameAtom, nameSpaceID);
  if (kNameSpaceID_Unknown == nameSpaceID) {
    nameSpaceID = kNameSpaceID_None;  // ignore unknown prefix XXX is this correct?
  }
  result = mContent->UnsetAttribute(nameSpaceID, nameAtom, PR_TRUE);
  NS_RELEASE(nameAtom);

  return result;
}

nsresult
nsGenericElement::GetAttributeNode(const nsString& aName,
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
      result = node->QueryInterface(kIDOMAttrIID, (void **)aReturn);
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
    result = aAttribute->QueryInterface(kIDOMNodeIID, (void **)&node);
    if (NS_OK == result) {
      result = map->SetNamedItem(node, &returnNode);
      if ((NS_OK == result) && (nsnull != returnNode)) {
        result = returnNode->QueryInterface(kIDOMAttrIID, (void **)aReturn);
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
        result = node->QueryInterface(kIDOMAttrIID, (void **)aReturn);
        NS_RELEASE(node);
      }
    }
    NS_RELEASE(map);
  }

  return result;
}

nsresult
nsGenericElement::GetElementsByTagName(const nsString& aTagname,
                                       nsIDOMNodeList** aReturn)
{
  nsIAtom* nameAtom;
  PRInt32 nameSpaceId;
  nsresult result = NS_OK;
  
  result = mContent->ParseAttributeString(aTagname, nameAtom,
                                          nameSpaceId);
  if (NS_OK != result) {
    return result;
  }

  nsContentList* list = new nsContentList(mDocument, 
                                          nameAtom, 
                                          nameSpaceId, 
                                          mContent);
  NS_IF_RELEASE(nameAtom);
  if (nsnull == list) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return list->QueryInterface(kIDOMNodeListIID, (void **)aReturn);
}

nsresult
nsGenericElement::JoinTextNodes(nsIContent* aFirst,
                                nsIContent* aSecond)
{
  nsresult result = NS_OK;
  nsIDOMText *firstText, *secondText;

  result = aFirst->QueryInterface(kIDOMTextIID, (void**)&firstText);
  if (NS_OK == result) {
    result = aSecond->QueryInterface(kIDOMTextIID, (void**)&secondText);

    if (NS_OK == result) {
      nsAutoString str;

      result = secondText->GetData(str);
      if (NS_OK == result) {
        result = firstText->AppendData(str);
      }
      
      NS_RELEASE(secondText);
    }
    
    NS_RELEASE(firstText);
  }
  
  return result;
}

nsresult
nsGenericElement::Normalize()
{
  nsresult result = NS_OK;
  PRInt32 index, count;

  mContent->ChildCount(count);
  for (index = 0; (index < count) && (NS_OK == result); index++) {
    nsCOMPtr<nsIContent> child;

    result = mContent->ChildAt(index, *getter_AddRefs(child));
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
            result = mContent->ChildAt(index+1, *getter_AddRefs(sibling));
            if (NS_FAILED(result)) {
              return result;
            }
            
            nsCOMPtr<nsIDOMNode> siblingNode = do_QueryInterface(sibling);
              
            if (sibling) {
              PRUint16 siblingNodeType;
              siblingNode->GetNodeType(&siblingNodeType);
              
              if (siblingNodeType == nsIDOMNode::TEXT_NODE) {
                result = mContent->RemoveChildAt(index+1, PR_TRUE);
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
                                          nsIDocument* aDocument)
{
  PRInt32 i, n;
  aContent->ChildCount(n);
  for (i = 0; i < n; i++) {
    nsIContent* child;
    aContent->ChildAt(i, child);
    if (nsnull != child) {
      child->SetDocument(aDocument, PR_TRUE);
      NS_RELEASE(child);
    }
  }
}

nsresult
nsGenericElement::SetDocument(nsIDocument* aDocument, PRBool aDeep)
{
  if (aDocument != mDocument) {
    // If we were part of a document, make sure we get rid of the
    // script context reference to our script object so that our
    // script object can be freed (or collected).
    if ((nsnull != mDocument) && (nsnull != mDOMSlots) &&
        (nsnull != mDOMSlots->mScriptObject)) {
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
    
    mDocument = aDocument;
    
    // If we already have a script object and now we're being added
    // to a document, make sure that the script context adds a 
    // reference to our script object. This will ensure that it
    // won't be freed (or collected) out from under us.
    if ((nsnull != mDocument) && (nsnull != mDOMSlots) &&
        (nsnull != mDOMSlots->mScriptObject)) {
      nsCOMPtr<nsIScriptGlobalObject> globalObject;
      mDocument->GetScriptGlobalObject(getter_AddRefs(globalObject));
      if (globalObject) {
        nsCOMPtr<nsIScriptContext> context;
        if (NS_OK == globalObject->GetContext(getter_AddRefs(context)) && context) {
          nsAutoString tag;
          char tagBuf[50];
          
          mTag->ToString(tag);
          tag.ToCString(tagBuf, sizeof(tagBuf));
          context->AddNamedReference((void *)&mDOMSlots->mScriptObject,
                                     mDOMSlots->mScriptObject,
                                     tagBuf);
        }
      }
    }
  }

  if (PR_TRUE == aDeep) {
    SetDocumentInChildrenOf(mContent, aDocument);
  }

  return NS_OK;
}


nsresult
nsGenericElement::GetParent(nsIContent*& aResult) const
{
  NS_IF_ADDREF(mParent);
  aResult = mParent;
  return NS_OK;
}

nsresult
nsGenericElement::SetParent(nsIContent* aParent)
{
  mParent = aParent;
  return NS_OK;
}

nsresult
nsGenericElement::GetNameSpaceID(PRInt32& aResult) const
{
  aResult = kNameSpaceID_None;
  return NS_OK;
}

nsresult
nsGenericElement::GetTag(nsIAtom*& aResult) const
{
  aResult = mTag;
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
  
  nsIDOMEvent* domEvent = nsnull;
  if (NS_EVENT_FLAG_INIT == aFlags) {
    aDOMEvent = &domEvent;
    aEvent->flags = NS_EVENT_FLAG_NONE;
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
  
  //Local handling stage
  if (mDOMSlots && mDOMSlots->mListenerManager && !(aEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH)) {
    aEvent->flags |= aFlags;
    mDOMSlots->mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, aFlags, aEventStatus);
    aEvent->flags &= ~aFlags;
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

  if (NS_EVENT_FLAG_INIT == aFlags) {
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
  
  
nsresult 
nsGenericElement::RangeAdd(nsIDOMRange& aRange)
{
  if (nsnull == mDOMSlots) GetDOMSlots();
  // lazy allocation of range list
  if (nsnull == mDOMSlots->mRangeList) {
    mDOMSlots->mRangeList = new nsVoidArray();
  }
  if (nsnull == mDOMSlots->mRangeList) {
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
  if (rv)  return NS_OK;
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
  return NS_OK;
}

nsresult 
nsGenericElement::RemoveFocus(nsIPresContext* aPresContext)
{
  return NS_OK;
}

nsresult
nsGenericElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult,
                         size_t aInstanceSize) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
#ifdef DEBUG
  *aResult = (PRUint32) aInstanceSize;
#else
  *aResult = 0;
#endif
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
    shell->GetPrimaryFrameFor(mContent, &frame);
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

  if (nsnull == slots->mScriptObject) {
    nsIDOMScriptObjectFactory *factory;
    
    res = GetScriptObjectFactory(&factory);
    if (NS_OK != res) {
      return res;
    }
    
    nsAutoString tag;
    mTag->ToString(tag);
    res = factory->NewScriptElement(tag, aContext, mContent,
                                    mParent ? (nsISupports*)mParent : (nsISupports*)mDocument,
                                    (void**)&slots->mScriptObject);
    NS_RELEASE(factory);
    
    char tagBuf[50];
    tag.ToCString(tagBuf, sizeof(tagBuf));
    if (nsnull != mDocument) {
      aContext->AddNamedReference((void *)&slots->mScriptObject,
                                  slots->mScriptObject,
                                  tagBuf);
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
  }
  return rv;
}

//----------------------------------------------------------------------

// nsIJSScriptObject implementation

PRBool    
nsGenericElement::AddProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
  return PR_TRUE;
}

PRBool    
nsGenericElement::DeleteProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
  return PR_TRUE;
}

PRBool    
nsGenericElement::GetProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
  return PR_TRUE;
}
 
PRBool    
nsGenericElement::SetProperty(JSContext *aContext, JSObject *aObj, jsval aID, jsval *aVp)
{
  nsIScriptObjectOwner *owner;

  if (NS_OK != mContent->QueryInterface(kIScriptObjectOwnerIID, (void **)&owner)) {
    return PR_FALSE;
  }

  if (JS_TypeOfValue(aContext, *aVp) == JSTYPE_FUNCTION && JSVAL_IS_STRING(aID)) {
    nsAutoString propName, prefix;
    propName.Assign(JS_GetStringChars(JS_ValueToString(aContext, aID)));
    if (propName.Length() > 2) 
      prefix.Assign(propName.GetUnicode(), 2);
    if (prefix.Equals("on")) {
      nsCOMPtr<nsIAtom> atom = getter_AddRefs(NS_NewAtom(propName));
      nsIEventListenerManager *manager = nsnull;

      if (atom.get() == nsLayoutAtoms::onmousedown || atom.get() == nsLayoutAtoms::onmouseup || atom.get() ==  nsLayoutAtoms::onclick ||
         atom.get() == nsLayoutAtoms::onmouseover || atom.get() == nsLayoutAtoms::onmouseout) {
        if (NS_OK == GetListenerManager(&manager)) {
          nsCOMPtr<nsIScriptContext> mScriptCX;
          if (NS_FAILED(nsLayoutUtils::GetStaticScriptContext(aContext, (JSObject*)GetDOMSlots()->mScriptObject, getter_AddRefs(mScriptCX))) ||
              NS_OK != manager->RegisterScriptEventListener(mScriptCX, owner, atom, kIDOMMouseListenerIID)) {
            NS_RELEASE(manager);
            return PR_FALSE;
          }
        }
      }
      else if (atom.get() == nsLayoutAtoms::onkeydown || atom.get() == nsLayoutAtoms::onkeyup || atom.get() == nsLayoutAtoms::onkeypress) {
        if (NS_OK == GetListenerManager(&manager)) {
          nsCOMPtr<nsIScriptContext> mScriptCX;
          if (NS_FAILED(nsLayoutUtils::GetStaticScriptContext(aContext, (JSObject*)GetDOMSlots()->mScriptObject, getter_AddRefs(mScriptCX))) ||
              NS_OK != manager->RegisterScriptEventListener(mScriptCX, owner, atom, kIDOMKeyListenerIID)) {
            NS_RELEASE(manager);
            return PR_FALSE;
          }
        }
      }
      else if (atom.get() == nsLayoutAtoms::onmousemove) {
        if (NS_OK == GetListenerManager(&manager)) {
          nsCOMPtr<nsIScriptContext> mScriptCX;
          if (NS_FAILED(nsLayoutUtils::GetStaticScriptContext(aContext, (JSObject*)GetDOMSlots()->mScriptObject, getter_AddRefs(mScriptCX))) ||
              NS_OK != manager->RegisterScriptEventListener(mScriptCX, owner, atom, kIDOMMouseMotionListenerIID)) {
            NS_RELEASE(manager);
            return PR_FALSE;
          }
        }
      }
      else if (atom.get() == nsLayoutAtoms::onfocus || atom.get() == nsLayoutAtoms::onblur) {
        if (NS_OK == GetListenerManager(&manager)) {
          nsCOMPtr<nsIScriptContext> mScriptCX;
          if (NS_FAILED(nsLayoutUtils::GetStaticScriptContext(aContext, (JSObject*)GetDOMSlots()->mScriptObject, getter_AddRefs(mScriptCX))) ||
              NS_OK != manager->RegisterScriptEventListener(mScriptCX, owner, atom, kIDOMFocusListenerIID)) {
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
              NS_OK != manager->RegisterScriptEventListener(mScriptCX, owner, atom, kIDOMFormListenerIID)) {
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
              NS_OK != manager->RegisterScriptEventListener(mScriptCX, owner, atom, kIDOMLoadListenerIID)) {
            NS_RELEASE(manager);
            return PR_FALSE;
          }
        }
      }
      else if (atom.get() == nsLayoutAtoms::onpaint) {
        if (NS_OK == GetListenerManager(&manager)) {
          nsCOMPtr<nsIScriptContext> mScriptCX;
          if (NS_FAILED(nsLayoutUtils::GetStaticScriptContext(aContext, (JSObject*)GetDOMSlots()->mScriptObject, getter_AddRefs(mScriptCX))) ||
              NS_OK != manager->RegisterScriptEventListener(mScriptCX, owner,
                                                            atom, kIDOMPaintListenerIID)) {
            NS_RELEASE(manager);
            return PR_FALSE;
          }
        }
      }
      NS_IF_RELEASE(manager);
    }
  }

  NS_IF_RELEASE(owner);

  return PR_TRUE;
}
 
PRBool    
nsGenericElement::EnumerateProperty(JSContext *aContext, JSObject *aObj)
{
  return PR_TRUE;
}

PRBool    
nsGenericElement::Resolve(JSContext *aContext, JSObject *aObj, jsval aID)
{
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

    mContent->IndexOf(refContent, refPos);

    if (refPos < 0) {
      return NS_ERROR_DOM_NOT_FOUND_ERR;
    }
  } else {
    mContent->ChildCount(refPos);
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
  if (isSelfOrAncestor(mContent, newContent)) {
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

      childContent->SetDocument(mDocument, PR_TRUE);

      // Insert the child and increment the insertion position
      res = mContent->InsertChildAt(childContent, refPos++, PR_TRUE);

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

      mContent->ChildCount(origChildCount);

      /*
       * We don't care here if the return fails or not.
       */
      oldParent->RemoveChild(aNewChild, getter_AddRefs(tmpNode));

      mContent->ChildCount(newChildCount);

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

          mContent->ChildAt(refPos - 1, *getter_AddRefs(tmpContent));

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

    newContent->SetDocument(mDocument, PR_TRUE);

    res = mContent->InsertChildAt(newContent, refPos, PR_TRUE);

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

  mContent->IndexOf(oldContent, oldPos);

  if (oldPos < 0) {
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  nsCOMPtr<nsIContent> replacedChild;

  mContent->ChildAt(oldPos, *getter_AddRefs(replacedChild));

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

  mContent->GetDocument(*getter_AddRefs(document));

  /*
   * Make sure the new child is not "this" node or one of this nodes
   * ancestors. Doing this check here should be safe even if newContent
   * is a document fragment.
   */
  if (isSelfOrAncestor(mContent, newContent)) {
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

      childContent->SetDocument(document, PR_TRUE);

      // Insert the child and increment the insertion position
      if (i) {
        res = mContent->InsertChildAt(childContent, oldPos++, PR_TRUE);
      } else {
        res = mContent->ReplaceChildAt(childContent, oldPos++, PR_TRUE);
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

      mContent->ChildCount(origChildCount);

      /*
       * We don't care here if the return fails or not.
       */
      oldParent->RemoveChild(aNewChild, getter_AddRefs(tmpNode));

      mContent->ChildCount(newChildCount);

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

        mContent->ChildAt(oldPos - 1, *getter_AddRefs(tmpContent));

        if (oldContent == tmpContent) {
          oldPos--;
        }
      }
    }

    newContent->SetDocument(document, PR_TRUE);

    res = mContent->ReplaceChildAt(newContent, oldPos, PR_TRUE);

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
  mContent->IndexOf(content, pos);

  if (pos < 0) {
    /*
     * aOldChild isn't one of our children.
     */
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  res = mContent->RemoveChildAt(pos, PR_TRUE);

  *aReturn = aOldChild;
  NS_ADDREF(aOldChild);

  return res;
}

//----------------------------------------------------------------------

// nsISupports implementation

nsresult
nsGenericElement::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  return mContent->QueryInterface(aIID, aInstancePtr);
}

nsrefcnt
nsGenericElement::AddRef()
{
  return NS_ADDREF(mContent);
}

nsrefcnt
nsGenericElement::Release()
{
  nsrefcnt rc=0;
  NS_ASSERTION(mContent, "nsGenericElement: Nothing to release!");
  if (mContent)
    NS_RELEASE2(mContent, rc);
  return rc;
}

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
    absURLSpec = aURLSpec;
  }

  // Now pass on absolute url to the click handler
  if (aClick) {
    nsresult proceed = NS_OK;
    // Check that this page is allowed to load this URI.
    NS_WITH_SERVICE(nsIScriptSecurityManager, securityManager, 
                    NS_SCRIPTSECURITYMANAGER_PROGID, &rv);
    nsCOMPtr<nsIURI> absURI;
    if (NS_SUCCEEDED(rv)) 
      rv = NS_NewURI(getter_AddRefs(absURI), absURLSpec, aBaseURL);
    if (NS_SUCCEEDED(rv)) 
      proceed = securityManager->CheckLoadURI(aBaseURL, absURI, PR_FALSE);

    // Only pass off the click event if the script security manager
    // says it's ok.
    if (NS_SUCCEEDED(proceed)) 
      handler->OnLinkClick(mContent, aVerb, absURLSpec.GetUnicode(), aTargetSpec.GetUnicode());
  }
  else {
    handler->OnOverLink(mContent, absURLSpec.GetUnicode(), aTargetSpec.GetUnicode());
  }
  return rv;
}

nsresult
nsGenericElement::AddScriptEventListener(nsIAtom* aAttribute,
                                         const nsString& aValue,
                                         REFNSIID aIID)
{
  nsresult ret = NS_OK;
  nsIScriptContext* context;

  if (nsnull != mDocument) {
    nsCOMPtr<nsIScriptGlobalObject> global;
    mDocument->GetScriptGlobalObject(getter_AddRefs(global));
    if (global) {
      if (NS_OK == global->GetContext(&context)) {
        if (nsHTMLAtoms::body == mTag || nsHTMLAtoms::frameset == mTag) {
          nsIDOMEventReceiver *receiver;

          if (nsnull != global && NS_OK == global->QueryInterface(kIDOMEventReceiverIID, (void**)&receiver)) {
            nsIEventListenerManager *manager;
            if (NS_OK == receiver->GetListenerManager(&manager)) {
              nsIScriptObjectOwner *mObjectOwner;
              if (NS_OK == global->QueryInterface(kIScriptObjectOwnerIID, (void**)&mObjectOwner)) {
                ret = manager->AddScriptEventListener(context, mObjectOwner, aAttribute, aValue, aIID, PR_FALSE);
                NS_RELEASE(mObjectOwner);
              }
              NS_RELEASE(manager);
            }
            NS_RELEASE(receiver);
          }
        }
        else {
          nsIEventListenerManager *manager;
          if (NS_OK == GetListenerManager(&manager)) {
            nsIScriptObjectOwner* cowner;
            if (NS_OK == mContent->QueryInterface(kIScriptObjectOwnerIID,
                                                  (void**) &cowner)) {
              ret = manager->AddScriptEventListener(context, cowner,
                                                    aAttribute, aValue, aIID, PR_TRUE);
              NS_RELEASE(cowner);
            }
            NS_RELEASE(manager);
          }
        }
        NS_RELEASE(context);
      }
    }
  }
  return ret;
}

static char kNameSpaceSeparator = ':';

nsIAtom*  
nsGenericElement::CutNameSpacePrefix(nsString& aString)
{
  nsAutoString  prefix;
  PRInt32 nsoffset = aString.FindChar(kNameSpaceSeparator);
  if (-1 != nsoffset) {
    aString.Left(prefix, nsoffset);
    aString.Cut(0, nsoffset+1);
  }
  if (0 < prefix.Length()) {
    return NS_NewAtom(prefix);
  }
  return nsnull;
}

//----------------------------------------------------------------------

struct nsGenericAttribute
{
  nsGenericAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, const nsString& aValue)
    : mNameSpaceID(aNameSpaceID),
      mName(aName),
      mValue(aValue)
  {
    NS_IF_ADDREF(mName);
  }

  ~nsGenericAttribute(void)
  {
    NS_IF_RELEASE(mName);
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

  PRInt32   mNameSpaceID;
  nsIAtom*  mName;
  nsString  mValue;
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
      result = aDst->SetAttribute(attr->mNameSpaceID, attr->mName,
                                  attr->mValue, PR_FALSE);
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
        result = child->QueryInterface(kIDOMNodeIID, (void**)&node);
        if (NS_OK == result) {
          nsIDOMNode* newNode;
          
          result = node->CloneNode(aDeep, &newNode);
          if (NS_OK == result) {
            nsIContent* newContent;
            
            result = newNode->QueryInterface(kIContentIID, (void**)&newContent);
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
    slots->mChildNodes = new nsChildContentList(mContent);
    if (nsnull == slots->mChildNodes) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(slots->mChildNodes);
  }

  return slots->mChildNodes->QueryInterface(kIDOMNodeListIID, (void **)aChildNodes);
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
    nsresult res = child->QueryInterface(kIDOMNodeIID, (void**)aNode);
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
    nsresult res = child->QueryInterface(kIDOMNodeIID, (void**)aNode);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Node"); // must be a DOM Node
    return res;
  }
  *aNode = nsnull;
  return NS_OK;
}

nsresult 
nsGenericContainerElement::SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, 
                                        const nsString& aValue,
                                        PRBool aNotify)
{
  NS_ASSERTION(kNameSpaceID_Unknown != aNameSpaceID, "must have name space ID");
  if (kNameSpaceID_Unknown == aNameSpaceID) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  NS_ASSERTION(nsnull != aName, "must have attribute name");
  if (nsnull == aName) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_ERROR_OUT_OF_MEMORY;

  if (nsnull == mAttributes) {
    mAttributes = new nsVoidArray();
  }
  
  if (aNotify && (nsnull != mDocument)) {
    mDocument->BeginUpdate();
  }
  
  if (nsnull != mAttributes) {
    nsGenericAttribute* attr;
    PRInt32 index;
    PRInt32 count = mAttributes->Count();
    for (index = 0; index < count; index++) {
      attr = (nsGenericAttribute*)mAttributes->ElementAt(index);
      if ((aNameSpaceID == attr->mNameSpaceID) && (aName == attr->mName)) {
        attr->mValue = aValue;
        rv = NS_OK;
        break;
      }
    }
    
    if (index >= count) { // didn't find it
      attr = new nsGenericAttribute(aNameSpaceID, aName, aValue);
      if (nsnull != attr) {
        mAttributes->AppendElement(attr);
        rv = NS_OK;
      }
    }
  }

  if (aNotify && (nsnull != mDocument)) {
    if (NS_SUCCEEDED(rv)) {
      mDocument->AttributeChanged(mContent, aNameSpaceID, aName, NS_STYLE_HINT_UNKNOWN);
    }
    mDocument->EndUpdate();
  }

  return rv;
}

nsresult 
nsGenericContainerElement::GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, 
                                        nsString& aResult) const
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
      if (((kNameSpaceID_Unknown == aNameSpaceID) || (attr->mNameSpaceID == aNameSpaceID)) && 
          (attr->mName == aName)) {
        aResult = attr->mValue;
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
nsGenericContainerElement::UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, 
                                          PRBool aNotify)
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
      if (((kNameSpaceID_Unknown == aNameSpaceID) || (attr->mNameSpaceID == aNameSpaceID)) && 
          (attr->mName == aName)) {
        if (aNotify && (nsnull != mDocument)) {
          mDocument->BeginUpdate();
        }
        mAttributes->RemoveElementAt(index);
        delete attr;
        found = PR_TRUE;
        break;
      }
    }

    if (NS_SUCCEEDED(rv) && found && aNotify && (nsnull != mDocument)) {
      mDocument->AttributeChanged(mContent, aNameSpaceID, aName, NS_STYLE_HINT_UNKNOWN);
      mDocument->EndUpdate();
    }
  }

  return rv;
}

nsresult
nsGenericContainerElement::GetAttributeNameAt(PRInt32 aIndex,
                                              PRInt32& aNameSpaceID, 
                                              nsIAtom*& aName) const
{
  if (nsnull != mAttributes) {
    nsGenericAttribute* attr = (nsGenericAttribute*)mAttributes->ElementAt(aIndex);
    if (nsnull != attr) {
      aNameSpaceID = attr->mNameSpaceID;
      aName = attr->mName;
      NS_IF_ADDREF(aName);
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

    if (kNameSpaceID_None != attr->mNameSpaceID) {  // prefix namespace
      buffer.Append(attr->mNameSpaceID, 10);
      buffer.Append(':');
    }

    // name
    nsAutoString name;
    attr->mName->ToString(name);
    buffer.Append(name);

    // value
    buffer.Append("=");
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

  nsIAtom* tag;
  GetTag(tag);
  if (tag != nsnull) {
    nsAutoString buf;
    tag->ToString(buf);
    fputs(buf, out);
    NS_RELEASE(tag);
  }
  fprintf(out, "@%p", mContent);

  ListAttributes(out);

  nsIContent* hc = mContent;  
  nsrefcnt r = NS_ADDREF(hc) - 1;
  NS_RELEASE(hc);
  fprintf(out, " refcount=%d<", r);

  PRBool canHaveKids;
  mContent->CanContainChildren(canHaveKids);
  if (canHaveKids) {
    fputs("\n", out);
    PRInt32 kids;
    mContent->ChildCount(kids);
    for (index = 0; index < kids; index++) {
      nsIContent* kid;
      mContent->ChildAt(index, kid);
      kid->List(out, aIndent + 1);
      NS_RELEASE(kid);
    }
    for (index = aIndent; --index >= 0; ) fputs("  ", out);
  }
  fputs(">\n", out);

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
    aKid->SetParent(mContent);
    nsRange::OwnerChildInserted(mContent, aIndex);
    if (nsnull != doc) {
      aKid->SetDocument(doc, PR_FALSE);
      if (aNotify) {
        doc->ContentInserted(mContent, aKid, aIndex);
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
  nsRange::OwnerChildReplaced(mContent, aIndex, oldKid);
  PRBool rv = mChildren.ReplaceElementAt(aKid, aIndex);
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(mContent);
    if (nsnull != doc) {
      aKid->SetDocument(doc, PR_FALSE);
      if (aNotify) {
        doc->ContentReplaced(mContent, oldKid, aKid, aIndex);
      }
    }
    oldKid->SetDocument(nsnull, PR_TRUE);
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
  NS_PRECONDITION((nsnull != aKid) && (aKid != mContent), "null ptr");
  nsIDocument* doc = mDocument;
  if (aNotify && (nsnull != doc)) {
    doc->BeginUpdate();
  }
  PRBool rv = mChildren.AppendElement(aKid);
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(mContent);
    // ranges don't need adjustment since new child is at end of list
    if (nsnull != doc) {
      aKid->SetDocument(doc, PR_FALSE);
      if (aNotify) {
        doc->ContentAppended(mContent, mChildren.Count() - 1);
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
    nsRange::OwnerChildRemoved(mContent, aIndex, oldKid);
    mChildren.RemoveElementAt(aIndex);
    if (aNotify) {
      if (nsnull != doc) {
        doc->ContentRemoved(mContent, oldKid, aIndex);
      }
    }
    oldKid->SetDocument(nsnull, PR_TRUE);
    oldKid->SetParent(nsnull);
    NS_RELEASE(oldKid);
    if (aNotify && (nsnull != doc)) {
      doc->EndUpdate();
    }
  }

  return NS_OK;
}

// XXX To be implemented
nsresult 
nsGenericContainerElement::BeginConvertToXIF(nsXIFConverter& aConverter) const
{
  return NS_OK;
}

nsresult 
nsGenericContainerElement::ConvertContentToXIF(nsXIFConverter& aConverter) const
{
  return NS_OK;
}
 
nsresult 
nsGenericContainerElement::FinishConvertToXIF(nsXIFConverter& aConverter) const
{
  return NS_OK;
}

nsresult
nsGenericContainerElement::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult,
                                  size_t aInstanceSize) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  PRUint32 sum = 0;
#ifdef DEBUG
  sum += aInstanceSize;
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
  *aResult = sum;
  return NS_OK;
}

