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
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsISizeOfHandler.h"
#include "nsISupportsArray.h"
#include "nsIURL.h"
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

#include "nsLayoutAtoms.h"
#include "nsHTMLAtoms.h"

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
  mListenerManager = nsnull;
}

nsGenericElement::~nsGenericElement()
{
  // pop any enclosed ranges out
  // nsRange::OwnerGone(mContent); not used for now
  NS_IF_RELEASE(mTag);
  NS_IF_RELEASE(mListenerManager);
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
  }
  
  return mDOMSlots;
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
nsGenericElement::GetNodeValue(nsString& aNodeValue)
{
  aNodeValue.Truncate();
  return NS_OK;
}

nsresult
nsGenericElement::SetNodeValue(const nsString& aNodeValue)
{
  return NS_OK;
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
    nsIContent* child;
    nsIAtom* name;

    result = mContent->ChildAt(index, child);
    if (NS_OK == result) {
      child->GetTag(name);

      // If this is a text node and there's a sibling following it
      if ((name == nsLayoutAtoms::textTagName) && (index < count-1)) {
        nsIContent* sibling;
        
        // Get the sibling. If it's also a text node, then
        // remove it from the tree and join the two text
        // nodes.
        result = mContent->ChildAt(index+1, sibling);
        if (NS_OK == result) {
          nsIAtom* siblingName;
          
          sibling->GetTag(siblingName);
          if (siblingName == nsLayoutAtoms::textTagName) {
            result = mContent->RemoveChildAt(index+1, PR_TRUE);
            if (NS_OK == result) {
              result = JoinTextNodes(child, sibling);
              count--;
            }
          }
          
          NS_IF_RELEASE(siblingName);
          NS_RELEASE(sibling);
        }
      }
      else {
        nsIDOMElement* element;
        nsresult qiresult;

        qiresult = child->QueryInterface(kIDOMElementIID, (void**)&element);
        if (NS_OK == qiresult) {
          result = element->Normalize();
          NS_RELEASE(element);
        }
      }

      NS_IF_RELEASE(name);
      NS_RELEASE(child);
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
  // If we were part of a document, make sure we get rid of the
  // script context reference to our script object so that our
  // script object can be freed (or collected).
  if ((nsnull != mDocument) && (nsnull != mDOMSlots) &&
      (nsnull != mDOMSlots->mScriptObject)) {
    nsIScriptContextOwner *owner = mDocument->GetScriptContextOwner();
    if (nsnull != owner) {
      nsIScriptContext *context;
      if (NS_OK == owner->GetScriptContext(&context)) {
        context->RemoveReference((void *)&mDOMSlots->mScriptObject,
                                mDOMSlots->mScriptObject);
        NS_RELEASE(context);
      }
      NS_RELEASE(owner);
    }
  }

  mDocument = aDocument;

  // If we already have a script object and now we're being added
  // to a document, make sure that the script context adds a 
  // reference to our script object. This will ensure that it
  // won't be freed (or collected) out from under us.
  if ((nsnull != mDocument) && (nsnull != mDOMSlots) &&
      (nsnull != mDOMSlots->mScriptObject)) {
    nsIScriptContextOwner *owner = mDocument->GetScriptContextOwner();
    if (nsnull != owner) {
      nsIScriptContext *context;
      if (NS_OK == owner->GetScriptContext(&context)) {
        nsAutoString tag;
        char tagBuf[50];
        
        mTag->ToString(tag);
        tag.ToCString(tagBuf, sizeof(tagBuf));
        context->AddNamedReference((void *)&mDOMSlots->mScriptObject,
                                   mDOMSlots->mScriptObject,
                                   tagBuf);
        NS_RELEASE(context);
      }
      NS_RELEASE(owner);
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
  return NS_OK;;
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
  NS_IF_ADDREF(mTag);
  aResult = mTag;
  return NS_OK;
}


nsresult
nsGenericElement::HandleDOMEvent(nsIPresContext& aPresContext,
                                     nsEvent* aEvent,
                                     nsIDOMEvent** aDOMEvent,
                                     PRUint32 aFlags,
                                     nsEventStatus& aEventStatus)
{
  nsresult ret = NS_OK;
  
  nsIDOMEvent* domEvent = nsnull;
  if (NS_EVENT_FLAG_INIT == aFlags) {
    aDOMEvent = &domEvent;

    //Initiate capturing phase
    //Initiate capturing phase.  Special case first call to document
    if (nsnull != mDocument) {
      mDocument->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, NS_EVENT_FLAG_CAPTURE, aEventStatus);
    }
  }
  
  //Capturing stage evaluation
  //Always pass capturing up the tree before local evaulation
  if (NS_EVENT_FLAG_BUBBLE != aFlags) {
    if (nsnull != mDOMSlots && nsnull != mDOMSlots->mCapturer) {
      mDOMSlots->mCapturer->HandleDOMEvent(aPresContext, aEvent, aDOMEvent, NS_EVENT_FLAG_CAPTURE, aEventStatus);
    } 
  }
  
  //Local handling stage
  if (nsnull != mListenerManager) {
    mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, aFlags, aEventStatus);
  }

  //Bubbling stage
  if ((NS_EVENT_FLAG_CAPTURE != aFlags) && (mParent != nsnull) && (mDocument != nsnull)) {
    ret = mParent->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                  NS_EVENT_FLAG_BUBBLE, aEventStatus);
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
        if ( (mDOMSlots->mScriptObject == nsnull) &&
             (mDOMSlots->mChildNodes == nsnull) &&
             (mDOMSlots->mStyle == nsnull) ) {
          PR_DELETE(mDOMSlots);
        }
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

//----------------------------------------------------------------------

nsresult
nsGenericElement::RenderFrame()
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
      frame->GetOffsetFromView(offset, &view);
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
                                    mParent, (void**)&slots->mScriptObject);
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
  return NS_OK;
}

//----------------------------------------------------------------------

// nsIDOMEventReceiver implementation

nsresult
nsGenericElement::GetListenerManager(nsIEventListenerManager** aResult)
{
  if (nsnull != mListenerManager) {
    NS_ADDREF(mListenerManager);
    *aResult = mListenerManager;
    return NS_OK;
  }
  nsresult rv = NS_NewEventListenerManager(aResult);
  if (NS_OK == rv) {
    mListenerManager = *aResult;
    NS_ADDREF(mListenerManager);
  }
  return rv;
}

nsresult
nsGenericElement::GetNewListenerManager(nsIEventListenerManager** aResult)
{
  return NS_NewEventListenerManager(aResult);
} 

nsresult
nsGenericElement::AddEventListenerByIID(nsIDOMEventListener* aListener,
                                   const nsIID& aIID)
{
  nsIEventListenerManager *manager;

  if (NS_OK == GetListenerManager(&manager)) {
    manager->AddEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
    NS_RELEASE(manager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult
nsGenericElement::RemoveEventListenerByIID(nsIDOMEventListener* aListener,
                                      const nsIID& aIID)
{
  if (nsnull != mListenerManager) {
    mListenerManager->RemoveEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult
nsGenericElement::AddEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                   PRBool aPostProcess, PRBool aUseCapture)
{
  nsIEventListenerManager *manager;

  if (NS_OK == GetListenerManager(&manager)) {
    PRInt32 flags = (aPostProcess ? NS_EVENT_FLAG_POST_PROCESS : NS_EVENT_FLAG_NONE) |
                    (aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE);

    manager->AddEventListenerByType(aListener, aType, flags);
    NS_RELEASE(manager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult
nsGenericElement::RemoveEventListener(const nsString& aType, nsIDOMEventListener* aListener, 
                                      PRBool aPostProcess, PRBool aUseCapture)
{
  if (nsnull != mListenerManager) {
    PRInt32 flags = (aPostProcess ? NS_EVENT_FLAG_POST_PROCESS : NS_EVENT_FLAG_NONE) |
                    (aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE);

    mListenerManager->RemoveEventListenerByType(aListener, aType, flags);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

// nsIJSScriptObject implementation

PRBool    
nsGenericElement::AddProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  return PR_TRUE;
}

PRBool    
nsGenericElement::DeleteProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  return PR_TRUE;
}

PRBool    
nsGenericElement::GetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  return PR_TRUE;
}
 
PRBool    
nsGenericElement::SetProperty(JSContext *aContext, jsval aID, jsval *aVp)
{
  nsIScriptObjectOwner *owner;

  if (NS_OK != mContent->QueryInterface(kIScriptObjectOwnerIID, (void **)&owner)) {
    return PR_FALSE;
  }

  if (JS_TypeOfValue(aContext, *aVp) == JSTYPE_FUNCTION && JSVAL_IS_STRING(aID)) {
    nsAutoString propName, prefix;
    propName.SetString(JS_GetStringChars(JS_ValueToString(aContext, aID)));
    prefix.SetString(propName.GetUnicode(), 2);
    if (prefix == "on") {
      nsIEventListenerManager *manager = nsnull;

      if (propName == "onmousedown" || propName == "onmouseup" || propName ==  "onclick" ||
         propName == "onmouseover" || propName == "onmouseout") {
        if (NS_OK == GetListenerManager(&manager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != manager->RegisterScriptEventListener(mScriptCX, owner, kIDOMMouseListenerIID)) {
            NS_RELEASE(manager);
            return PR_FALSE;
          }
        }
      }
      else if (propName == "onkeydown" || propName == "onkeyup" || propName == "onkeypress") {
        if (NS_OK == GetListenerManager(&manager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != manager->RegisterScriptEventListener(mScriptCX, owner, kIDOMKeyListenerIID)) {
            NS_RELEASE(manager);
            return PR_FALSE;
          }
        }
      }
      else if (propName == "onmousemove") {
        if (NS_OK == GetListenerManager(&manager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != manager->RegisterScriptEventListener(mScriptCX, owner, kIDOMMouseMotionListenerIID)) {
            NS_RELEASE(manager);
            return PR_FALSE;
          }
        }
      }
      else if (propName == "onfocus" || propName == "onblur") {
        if (NS_OK == GetListenerManager(&manager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != manager->RegisterScriptEventListener(mScriptCX, owner, kIDOMFocusListenerIID)) {
            NS_RELEASE(manager);
            return PR_FALSE;
          }
        }
      }
      else if (propName == "onsubmit" || propName == "onreset" || propName == "onchange") {
        if (NS_OK == GetListenerManager(&manager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != manager->RegisterScriptEventListener(mScriptCX, owner, kIDOMFormListenerIID)) {
            NS_RELEASE(manager);
            return PR_FALSE;
          }
        }
      }
      else if (propName == "onload" || propName == "onunload" || propName == "onabort" ||
               propName == "onerror") {
        if (NS_OK == GetListenerManager(&manager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)JS_GetContextPrivate(aContext);
          if (NS_OK != manager->RegisterScriptEventListener(mScriptCX, owner, kIDOMLoadListenerIID)) {
            NS_RELEASE(manager);
            return PR_FALSE;
          }
        }
      }
      else if (propName == "onpaint") {
        if (NS_OK == GetListenerManager(&manager)) {
          nsIScriptContext *mScriptCX = (nsIScriptContext *)
            JS_GetContextPrivate(aContext);
          if (NS_OK != manager->RegisterScriptEventListener(mScriptCX, owner,
                                                    kIDOMPaintListenerIID)) {
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
nsGenericElement::EnumerateProperty(JSContext *aContext)
{
  return PR_TRUE;
}

PRBool    
nsGenericElement::Resolve(JSContext *aContext, jsval aID)
{
  return PR_TRUE;
}

PRBool    
nsGenericElement::Convert(JSContext *aContext, jsval aID)
{
  return PR_TRUE;
}

void      
nsGenericElement::Finalize(JSContext *aContext)
{
}
 
//----------------------------------------------------------------------

// nsISupports implementation

NS_IMETHODIMP
nsGenericElement::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  return mContent->QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP_(nsrefcnt) 
nsGenericElement::AddRef()
{
  return NS_ADDREF(mContent);
}

NS_IMETHODIMP_(nsrefcnt) 
nsGenericElement::Release()
{
  nsrefcnt rc;
  NS_RELEASE2(mContent, rc);
  return rc;
}

//----------------------------------------------------------------------

void
nsGenericElement::TriggerLink(nsIPresContext& aPresContext,
                              nsLinkVerb aVerb,
                              nsIURL* aBaseURL,
                              const nsString& aURLSpec,
                              const nsString& aTargetSpec,
                              PRBool aClick)
{
  nsILinkHandler* handler;
  nsresult rv = aPresContext.GetLinkHandler(&handler);
  if (NS_SUCCEEDED(rv) && (nsnull != handler)) {
    // Resolve url to an absolute url
    nsAutoString absURLSpec;
    if (nsnull != aBaseURL) {
      nsString empty;
      NS_MakeAbsoluteURL(aBaseURL, empty, aURLSpec, absURLSpec);
    }
    else {
      absURLSpec = aURLSpec;
    }

    // Now pass on absolute url to the click handler
    if (aClick) {
      handler->OnLinkClick(mContent, aVerb, absURLSpec.GetUnicode(), aTargetSpec.GetUnicode());
    }
    else {
      handler->OnOverLink(mContent, absURLSpec.GetUnicode(), aTargetSpec.GetUnicode());
    }
    NS_RELEASE(handler);
  }
}

nsresult
nsGenericElement::AddScriptEventListener(nsIAtom* aAttribute,
                                             const nsString& aValue,
                                             REFNSIID aIID)
{
  nsresult ret = NS_OK;
  nsIScriptContext* context;
  nsIScriptContextOwner* owner;

  if (nsnull != mDocument) {
    owner = mDocument->GetScriptContextOwner();
    if (NS_OK == owner->GetScriptContext(&context)) {
      if (nsHTMLAtoms::body == mTag || nsHTMLAtoms::frameset == mTag) {
        nsIDOMEventReceiver *receiver;
        nsIScriptGlobalObject *global = context->GetGlobalObject();

        if (nsnull != global && NS_OK == global->QueryInterface(kIDOMEventReceiverIID, (void**)&receiver)) {
          nsIEventListenerManager *manager;
          if (NS_OK == receiver->GetListenerManager(&manager)) {
            nsIScriptObjectOwner *mObjectOwner;
            if (NS_OK == global->QueryInterface(kIScriptObjectOwnerIID, (void**)&mObjectOwner)) {
              ret = manager->AddScriptEventListener(context, mObjectOwner, aAttribute, aValue, aIID);
              NS_RELEASE(mObjectOwner);
            }
            NS_RELEASE(manager);
          }
          NS_RELEASE(receiver);
        }
        NS_IF_RELEASE(global);
      }
      else {
        nsIEventListenerManager *manager;
        if (NS_OK == GetListenerManager(&manager)) {
          nsIScriptObjectOwner* owner;
          if (NS_OK == mContent->QueryInterface(kIScriptObjectOwnerIID,
                                                (void**) &owner)) {
            ret = manager->AddScriptEventListener(context, owner,
                                                  aAttribute, aValue, aIID);
            NS_RELEASE(owner);
          }
          NS_RELEASE(manager);
        }
      }
      NS_RELEASE(context);
    }
    NS_RELEASE(owner);
  }
  return ret;
}

static char kNameSpaceSeparator[] = ":";

nsIAtom*  
nsGenericElement::CutNameSpacePrefix(nsString& aString)
{
  nsAutoString  prefix;
  PRInt32 nsoffset = aString.Find(kNameSpaceSeparator);
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
nsGenericContainerElement::InsertBefore(nsIDOMNode* aNewChild,
                                        nsIDOMNode* aRefChild,
                                        nsIDOMNode** aReturn)
{
  nsresult res;

  *aReturn = nsnull;
  if (nsnull == aNewChild) {
    return NS_OK;/* XXX wrong error value */
  }

  // Check if this is a document fragment. If it is, we need
  // to remove the children of the document fragment and add them
  // individually (i.e. we don't add the actual document fragment).
  nsIDOMDocumentFragment* docFrag = nsnull;
  if (NS_OK == aNewChild->QueryInterface(kIDOMDocumentFragmentIID,
                                         (void **)&docFrag)) {
    
    nsIContent* docFragContent;
    res = aNewChild->QueryInterface(kIContentIID, (void **)&docFragContent);

    if (NS_OK == res) {
      nsIContent* refContent = nsnull;
      nsIContent* childContent = nsnull;
      PRInt32 refPos = 0;
      PRInt32 i, count;

      if (nsnull != aRefChild) {
        res = aRefChild->QueryInterface(kIContentIID, (void **)&refContent);
        NS_ASSERTION(NS_OK == res, "Ref child must be an nsIContent");
        IndexOf(refContent, refPos);
      }

      docFragContent->ChildCount(count);
      // Iterate through the fragments children, removing each from
      // the fragment and inserting it into the child list of its
      // new parent.
      for (i = 0; i < count; i++) {
        // Always get and remove the first child, since the child indexes
        // change as we go along.
        res = docFragContent->ChildAt(0, childContent);
        if (NS_OK == res) {
          res = docFragContent->RemoveChildAt(0, PR_FALSE);
          if (NS_OK == res) {
            SetDocumentInChildrenOf(childContent, mDocument);
            if (nsnull == refContent) {
              // Append the new child to the end
              res = AppendChildTo(childContent, PR_TRUE);
            }
            else {
              // Insert the child and increment the insertion position
              res = InsertChildAt(childContent, refPos++, PR_TRUE);
            }
            if (NS_OK != res) {
              // Stop inserting and indicate failure
              break;
            }
          }
          else {
            // Stop inserting and indicate failure
            break;
          }
        }
        else {
          // Stop inserting and indicate failure
          break;
        }
      }
      NS_RELEASE(docFragContent);
      
      // XXX Should really batch notification till the end
      // rather than doing it for each element added
      if (NS_OK == res) {
        *aReturn = aNewChild;
        NS_ADDREF(aNewChild);
      }
      NS_IF_RELEASE(refContent);
    }
    NS_RELEASE(docFrag);
  }
  else {
    // Get the nsIContent interface for the new content
    nsIContent* newContent = nsnull;
    res = aNewChild->QueryInterface(kIContentIID, (void**)&newContent);
    NS_ASSERTION(NS_OK == res, "New child must be an nsIContent");
    if (NS_OK == res) {
      nsIContent* oldParent;
      res = newContent->GetParent(oldParent);
      if (NS_OK == res) {
        // Remove the element from the old parent if one exists
        if (nsnull != oldParent) {
          PRInt32 index;
          oldParent->IndexOf(newContent, index);
          if (-1 != index) {
            oldParent->RemoveChildAt(index, PR_TRUE);
          }
          NS_RELEASE(oldParent);
        }

        if (nsnull == aRefChild) {
          // Append the new child to the end
          SetDocumentInChildrenOf(newContent, mDocument);
          res = AppendChildTo(newContent, PR_TRUE);
        }
        else {
          // Get the index of where to insert the new child
          nsIContent* refContent = nsnull;
          res = aRefChild->QueryInterface(kIContentIID, (void**)&refContent);
          NS_ASSERTION(NS_OK == res, "Ref child must be an nsIContent");
          if (NS_OK == res) {
            PRInt32 pos;
            IndexOf(refContent, pos);
            if (pos >= 0) {
              SetDocumentInChildrenOf(newContent, mDocument);
              res = InsertChildAt(newContent, pos, PR_TRUE);
            }
            NS_RELEASE(refContent);
          }
        }
      }
      NS_RELEASE(newContent);

      *aReturn = aNewChild;
      NS_ADDREF(aNewChild);
    }
  }

  return res;
}

nsresult
nsGenericContainerElement::ReplaceChild(nsIDOMNode* aNewChild,
                                        nsIDOMNode* aOldChild,
                                        nsIDOMNode** aReturn)
{
  *aReturn = nsnull;
  if (nsnull == aOldChild) {
    return NS_OK;
  }
  nsIContent* content = nsnull;
  nsresult res = aOldChild->QueryInterface(kIContentIID, (void**)&content);
  NS_ASSERTION(NS_OK == res, "Must be an nsIContent");
  if (NS_OK == res) {
    PRInt32 pos;
    IndexOf(content, pos);
    if (pos >= 0) {
      nsIContent* newContent = nsnull;
      nsresult res = aNewChild->QueryInterface(kIContentIID, (void**)&newContent);
      NS_ASSERTION(NS_OK == res, "Must be an nsIContent");
      if (NS_OK == res) {
        // Check if this is a document fragment. If it is, we need
        // to remove the children of the document fragment and add them
        // individually (i.e. we don't add the actual document fragment).
        nsIDOMDocumentFragment* docFrag = nsnull;
        if (NS_OK == aNewChild->QueryInterface(kIDOMDocumentFragmentIID,
                                               (void **)&docFrag)) {
    
          nsIContent* docFragContent;
          res = aNewChild->QueryInterface(kIContentIID, (void **)&docFragContent);
          if (NS_OK == res) {
            PRInt32 count;

            docFragContent->ChildCount(count);
            // If there are children of the document
            if (count > 0) {
              nsIContent* childContent;
              // Remove the last child of the document fragment
              // and do a replace with it
              res = docFragContent->ChildAt(count-1, childContent);
              if (NS_OK == res) {
                res = docFragContent->RemoveChildAt(count-1, PR_FALSE);
                if (NS_OK == res) {
                  SetDocumentInChildrenOf(childContent, mDocument);
                  res = ReplaceChildAt(childContent, pos, PR_TRUE);
                  // If there are more children, then insert them before
                  // the newly replaced child
                  if ((NS_OK == res) && (count > 1)) {
                    nsIDOMNode* childNode = nsnull;

                    res = childContent->QueryInterface(kIDOMNodeIID,
                                                       (void **)&childNode);
                    if (NS_OK == res) {
                      nsIDOMNode* rv;

                      res = InsertBefore(aNewChild, childNode, &rv);
                      if (NS_OK == res) {
                        NS_IF_RELEASE(rv);
                      }
                      NS_RELEASE(childNode);
                    }
                  }
                }
                NS_RELEASE(childContent);
              }
            }
            NS_RELEASE(docFragContent);
          }
          NS_RELEASE(docFrag);
        }
        else {
          nsIContent* oldParent;
          res = newContent->GetParent(oldParent);
          if (NS_OK == res) {
            // Remove the element from the old parent if one exists
            if (nsnull != oldParent) {
              PRInt32 index;
              oldParent->IndexOf(newContent, index);
              if (-1 != index) {
                oldParent->RemoveChildAt(index, PR_TRUE);
              }
              NS_RELEASE(oldParent);
            }
           
            SetDocumentInChildrenOf(newContent, mDocument);
            res = ReplaceChildAt(newContent, pos, PR_TRUE);
          }
        }
        NS_RELEASE(newContent);
      }
      *aReturn = aOldChild;
      NS_ADDREF(aOldChild);
    }
    NS_RELEASE(content);
  }
  
  return res;
}

nsresult
nsGenericContainerElement::RemoveChild(nsIDOMNode* aOldChild, 
                                       nsIDOMNode** aReturn)
{
  nsIContent* content = nsnull;
  *aReturn = nsnull;
  nsresult res = aOldChild->QueryInterface(kIContentIID, (void**)&content);
  NS_ASSERTION(NS_OK == res, "Must be an nsIContent");
  if (NS_OK == res) {
    PRInt32 pos;
    IndexOf(content, pos);
    if (pos >= 0) {
      res = RemoveChildAt(pos, PR_TRUE);
      *aReturn = aOldChild;
      NS_ADDREF(aOldChild);
    }
    NS_RELEASE(content);
  }

  return res;
}

nsresult
nsGenericContainerElement::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
  return InsertBefore(aNewChild, nsnull, aReturn);
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

  if (NS_SUCCEEDED(rv) && aNotify && (nsnull != mDocument)) {
    mDocument->AttributeChanged(mContent, aName, NS_STYLE_HINT_UNKNOWN);
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
        mAttributes->RemoveElementAt(index);
        delete attr;
        found = PR_TRUE;
        break;
      }
    }

    if (NS_SUCCEEDED(rv) && found && aNotify && (nsnull != mDocument)) {
      mDocument->AttributeChanged(mContent, aName, NS_STYLE_HINT_UNKNOWN);
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
  PRBool rv = mChildren.InsertElementAt(aKid, aIndex);/* XXX fix up void array api to use nsresult's*/
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(mContent);
    nsRange::OwnerChildInserted(mContent, aIndex);
    nsIDocument* doc = mDocument;
    if (nsnull != doc) {
      aKid->SetDocument(doc, PR_FALSE);
      if (aNotify) {
        doc->ContentInserted(mContent, aKid, aIndex);
      }
    }
  }
  return NS_OK;
}

nsresult
nsGenericContainerElement::ReplaceChildAt(nsIContent* aKid,
                                          PRInt32 aIndex,
                                          PRBool aNotify)
{
  NS_PRECONDITION(nsnull != aKid, "null ptr");
  nsIContent* oldKid = (nsIContent *)mChildren.ElementAt(aIndex);
  PRBool rv = mChildren.ReplaceElementAt(aKid, aIndex);
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(mContent);
    nsRange::OwnerChildReplaced(mContent, aIndex, oldKid);
    nsIDocument* doc = mDocument;
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
  return NS_OK;
}

nsresult
nsGenericContainerElement::AppendChildTo(nsIContent* aKid, PRBool aNotify)
{
  NS_PRECONDITION((nsnull != aKid) && (aKid != mContent), "null ptr");
  PRBool rv = mChildren.AppendElement(aKid);
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(mContent);
    // ranges don't need adjustment since new child is at end of list
    nsIDocument* doc = mDocument;
    if (nsnull != doc) {
      aKid->SetDocument(doc, PR_FALSE);
      if (aNotify) {
        doc->ContentAppended(mContent, mChildren.Count() - 1);
      }
    }
  }
  return NS_OK;
}

nsresult
nsGenericContainerElement::RemoveChildAt(PRInt32 aIndex, PRBool aNotify)
{
  nsIContent* oldKid = (nsIContent *)mChildren.ElementAt(aIndex);
  if (nsnull != oldKid ) {
    nsIDocument* doc = mDocument;
    mChildren.RemoveElementAt(aIndex);
    nsRange::OwnerChildRemoved(mContent, aIndex, oldKid);
    if (aNotify) {
      if (nsnull != doc) {
        doc->ContentRemoved(mContent, oldKid, aIndex);
      }
    }
    oldKid->SetDocument(nsnull, PR_TRUE);
    oldKid->SetParent(nsnull);
    NS_RELEASE(oldKid);
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
