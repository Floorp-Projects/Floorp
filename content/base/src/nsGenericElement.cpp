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


#include "nsIAtom.h"
#include "nsIDocument.h"
#include "nsIDOMAttr.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMRange.h"
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
#include "prprf.h"
#include "prmem.h"

#include "nsHTMLAtoms.h"
#include "nsIHTMLAttributes.h"

NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
NS_DEFINE_IID(kIDOMEventReceiverIID, NS_IDOMEVENTRECEIVER_IID);
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

nsDOMAttribute::nsDOMAttribute(const nsString& aName, const nsString& aValue)
  : mName(aName), mValue(aValue)
{
  mRefCnt = 1;
  mScriptObject = nsnull;
}

nsDOMAttribute::~nsDOMAttribute()
{
}

nsresult
nsDOMAttribute::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDOMAttrIID)) {
    nsIDOMAttr* tmp = this;
    *aInstancePtr = (void*)tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    nsIScriptObjectOwner* tmp = this;
    *aInstancePtr = (void*)tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsIDOMAttr* tmp1 = this;
    nsISupports* tmp2 = tmp1;
    *aInstancePtr = (void*)tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsDOMAttribute)

NS_IMPL_RELEASE(nsDOMAttribute)

nsresult
nsDOMAttribute::GetScriptObject(nsIScriptContext *aContext,
                                void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptAttr(aContext, 
                           (nsISupports *)(nsIDOMAttr *)this, 
                           nsnull,
                           (void **)&mScriptObject);
  }
  *aScriptObject = mScriptObject;
  return res;
}

nsresult
nsDOMAttribute::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

nsresult
nsDOMAttribute::GetName(nsString& aName)
{
  aName = mName;
  return NS_OK;
}

nsresult
nsDOMAttribute::GetValue(nsString& aValue)
{
  aValue = mValue;
  return NS_OK;
}

nsresult
nsDOMAttribute::SetValue(const nsString& aValue)
{
  mValue = aValue;
  return NS_OK;
}

nsresult
nsDOMAttribute::GetSpecified(PRBool* aSpecified)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMAttribute::GetNodeName(nsString& aNodeName)
{
  return GetName(aNodeName);
}

NS_IMETHODIMP
nsDOMAttribute::GetNodeValue(nsString& aNodeValue)
{
  return GetValue(aNodeValue);
}

NS_IMETHODIMP
nsDOMAttribute::SetNodeValue(const nsString& aNodeValue)
{
  // You can't actually do this, but we'll fail silently
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::ATTRIBUTE_NODE;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetParentNode(nsIDOMNode** aParentNode)
{
  *aParentNode = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  *aChildNodes = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::HasChildNodes(PRBool* aHasChildNodes)
{
  *aHasChildNodes = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetFirstChild(nsIDOMNode** aFirstChild)
{
  *aFirstChild = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetLastChild(nsIDOMNode** aLastChild)
{
  *aLastChild = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetPreviousSibling(nsIDOMNode** aPreviousSibling)
{
  *aPreviousSibling = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetNextSibling(nsIDOMNode** aNextSibling)
{
  *aNextSibling = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
  *aAttributes = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttribute::InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDOMAttribute::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDOMAttribute::RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDOMAttribute::AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsDOMAttribute::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsDOMAttribute* newAttr = new nsDOMAttribute(mName, mValue);
  if (nsnull == newAttr) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aReturn = newAttr;
  return NS_OK;
}

NS_IMETHODIMP 
nsDOMAttribute::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  // XXX TBI
  return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------

nsDOMAttributeMap::nsDOMAttributeMap(nsIContent* aContent)
  : mContent(aContent)
{
  mRefCnt = 1;
  NS_ADDREF(mContent);
  mScriptObject = nsnull;
}

nsDOMAttributeMap::~nsDOMAttributeMap()
{
  NS_RELEASE(mContent);
}

nsresult
nsDOMAttributeMap::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDOMNamedNodeMapIID)) {
    nsIDOMNamedNodeMap* tmp = this;
    *aInstancePtr = (void*)tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    nsIScriptObjectOwner* tmp = this;
    *aInstancePtr = (void*)tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsIDOMNamedNodeMap* tmp1 = this;
    nsISupports* tmp2 = tmp1;
    *aInstancePtr = (void*)tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsDOMAttributeMap)

NS_IMPL_RELEASE(nsDOMAttributeMap)

nsresult
nsDOMAttributeMap::GetScriptObject(nsIScriptContext *aContext,
                                   void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptNamedNodeMap(aContext, 
                                   (nsISupports *)(nsIDOMNamedNodeMap *)this, 
                                   nsnull,
                                   (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;
  return res;
}

nsresult
nsDOMAttributeMap::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

nsresult
nsDOMAttributeMap::GetNamedItem(const nsString &aAttrName,
                              nsIDOMNode** aAttribute)
{
  nsAutoString value;
  mContent->GetAttribute(aAttrName, value);
  *aAttribute  = (nsIDOMNode *) new nsDOMAttribute(aAttrName, value);
  return NS_OK;
}

nsresult
nsDOMAttributeMap::SetNamedItem(nsIDOMNode *aNode, nsIDOMNode **aReturn)
{
  nsIDOMAttr *attribute;
  nsAutoString name, value;
  nsresult err;

  if (NS_OK != (err = aNode->QueryInterface(kIDOMAttrIID,
                                            (void **)&attribute))) {
    return err;
  }

  attribute->GetName(name);
  attribute->GetValue(value);
  NS_RELEASE(attribute);

  mContent->SetAttribute(name, value, PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMAttributeMap::RemoveNamedItem(const nsString& aName, nsIDOMNode** aReturn)
{
  nsresult res = GetNamedItem(aName, aReturn);
  if (NS_OK == res) {
    nsAutoString upper;
    aName.ToUpperCase(upper);
    nsIAtom* attr = NS_NewAtom(upper);
    mContent->UnsetAttribute(attr, PR_TRUE);
  }

  return res;
}

nsresult
nsDOMAttributeMap::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  nsresult res = NS_ERROR_FAILURE;
  nsAutoString name, value;
  nsISupportsArray *attributes = nsnull;
  if (NS_OK == NS_NewISupportsArray(&attributes)) {
    PRInt32 count;
    mContent->GetAllAttributeNames(attributes, count);
    if (count > 0) {
      if ((PRInt32)aIndex < count) {
        nsISupports *att = attributes->ElementAt(aIndex);
        static NS_DEFINE_IID(kIAtom, NS_IATOM_IID);
        nsIAtom *atName = nsnull;
        if (nsnull != att && NS_OK == att->QueryInterface(kIAtom, (void**)&atName)) {
          atName->ToString(name);
          if (NS_CONTENT_ATTR_NOT_THERE != mContent->GetAttribute(name, value)) {
            *aReturn = (nsIDOMNode *)new nsDOMAttribute(name, value);
            res = NS_OK;
          }
          NS_RELEASE(atName);
        }
      }
    }
    NS_RELEASE(attributes);
  }

  return res;
}

nsresult
nsDOMAttributeMap::GetLength(PRUint32 *aLength)
{
  PRInt32 n;
  nsresult rv = mContent->GetAttributeCount(n);
  *aLength = PRUint32(n);
  return rv;
}

//----------------------------------------------------------------------

nsChildContentList::nsChildContentList(nsIContent *aContent)
{
  NS_INIT_REFCNT();
  // This reference is not reference-counted. The content
  // object tells us when its about to go away.
  mContent = aContent;
  mScriptObject = nsnull;
}

NS_IMPL_ADDREF(nsChildContentList);
NS_IMPL_RELEASE(nsChildContentList);

nsresult
nsChildContentList::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDOMNodeListIID)) {
    nsIDOMNodeList* tmp = this;
    *aInstancePtr = (void*)tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    nsIScriptObjectOwner* tmp = this;
    *aInstancePtr = (void*)tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsIDOMNodeList* tmp1 = this;
    nsISupports* tmp2 = tmp1;
    *aInstancePtr = (void*)tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult 
nsChildContentList::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptNodeList(aContext, (nsISupports *)(nsIDOMNodeList *)this, mContent, (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;
  return res;
}

nsresult 
nsChildContentList::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
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
  mRangeList = nsnull;
}

nsGenericElement::~nsGenericElement()
{
  NS_IF_RELEASE(mTag);
  NS_IF_RELEASE(mListenerManager);
  if (nsnull != mDOMSlots) {
    if (nsnull != mDOMSlots->mChildNodes) {
      mDOMSlots->mChildNodes->DropReference();
      NS_RELEASE(mDOMSlots->mChildNodes);
    }
    if (nsnull != mDOMSlots->mStyle) {
      mDOMSlots->mStyle->DropReference();
      NS_RELEASE(mDOMSlots->mStyle);
    } 
    // XXX Should really be arena managed
    PR_DELETE(mDOMSlots);
  }
  delete mRangeList;
}

nsDOMSlots *
nsGenericElement::GetDOMSlots()
{
  if (nsnull == mDOMSlots) {
    mDOMSlots = PR_NEW(nsDOMSlots);
    mDOMSlots->mScriptObject = nsnull;
    mDOMSlots->mChildNodes = nsnull;
    mDOMSlots->mStyle = nsnull;
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

nsresult
nsGenericElement::GetPreviousSibling(nsIDOMNode** aNode)
{
  if (nsnull != mParent) {
    PRInt32 pos;
    mParent->IndexOf(mContent, pos);
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

nsresult
nsGenericElement::GetNextSibling(nsIDOMNode** aNextSibling)
{
  if (nsnull != mParent) {
    PRInt32 pos;
    mParent->IndexOf(mContent, pos);
    if (pos > -1 ) {
      nsIContent* prev;
      mParent->ChildAt(++pos, prev);
      if (nsnull != prev) {
        nsresult res = prev->QueryInterface(kIDOMNodeIID,(void**)aNextSibling);
        NS_ASSERTION(NS_OK == res, "Must be a DOM Node");
        NS_RELEASE(prev); // balance the AddRef in ChildAt()
        return res;
      }
    }
  }
  *aNextSibling = nsnull;
  return NS_OK;
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
  // XXX Should we create a new one every time or should we
  // cache one after we create it? If we find that this is
  // something that's called often, we might need to do the
  // latter.
  *aAttributes = new nsDOMAttributeMap(mContent);

  return NS_OK;
}

nsresult
nsGenericElement::GetTagName(nsString& aTagName)
{
  aTagName.Truncate();
  if (nsnull != mTag) {
    mTag->ToString(aTagName);
  }
  return NS_OK;
}

nsresult
nsGenericElement::GetDOMAttribute(const nsString& aName, nsString& aReturn)
{
  mContent->GetAttribute(aName, aReturn);
  return NS_OK;
}

nsresult
nsGenericElement::SetDOMAttribute(const nsString& aName,
                                      const nsString& aValue)
{
  mContent->SetAttribute(aName, aValue, PR_TRUE);
  return NS_OK;
}

nsresult
nsGenericElement::RemoveAttribute(const nsString& aName)
{
  nsAutoString upper;
  aName.ToUpperCase(upper);
  nsIAtom* attr = NS_NewAtom(upper);
  mContent->UnsetAttribute(attr, PR_TRUE);
  NS_RELEASE(attr);
  return NS_OK;
}

nsresult
nsGenericElement::GetAttributeNode(const nsString& aName,
                                       nsIDOMAttr** aReturn)
{
  nsAutoString value;
  if (NS_CONTENT_ATTR_NOT_THERE != mContent->GetAttribute(aName, value)) {
    *aReturn = new nsDOMAttribute(aName, value);
  }
  return NS_OK;
}

nsresult
nsGenericElement::SetAttributeNode(nsIDOMAttr* aAttribute, 
                                       nsIDOMAttr** aReturn)
{
  NS_PRECONDITION(nsnull != aAttribute, "null attribute");

  nsresult res = NS_ERROR_FAILURE;

  if (nsnull != aAttribute) {
    nsAutoString name, value;
    res = aAttribute->GetName(name);
    if (NS_OK == res) {
      res = aAttribute->GetValue(value);
      if (NS_OK == res) {
        mContent->SetAttribute(name, value, PR_TRUE);
      }
    }
  }
  return res;
}

nsresult
nsGenericElement::RemoveAttributeNode(nsIDOMAttr* aAttribute, 
                                          nsIDOMAttr** aReturn)
{
  NS_PRECONDITION(nsnull != aAttribute, "null attribute");

  nsresult res = NS_ERROR_FAILURE;

  if (nsnull != aAttribute) {
    nsAutoString name;
    res = aAttribute->GetName(name);
    if (NS_OK == res) {
      nsAutoString upper;
      name.ToUpperCase(upper);
      nsIAtom* attr = NS_NewAtom(upper);
      mContent->UnsetAttribute(attr, PR_TRUE);
    }
  }

  return res;
}

nsresult
nsGenericElement::GetElementsByTagName(const nsString& aTagname,
                                           nsIDOMNodeList** aReturn)
{
  return NS_ERROR_NOT_IMPLEMENTED;/* XXX */
}

nsresult
nsGenericElement::Normalize()
{
  return NS_ERROR_NOT_IMPLEMENTED;/* XXX */
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
  if (DOM_EVENT_INIT == aFlags) {
    aDOMEvent = &domEvent;
  }
  
  //Capturing stage
  
  //Local handling stage
  if (nsnull != mListenerManager) {
    mListenerManager->HandleEvent(aPresContext, aEvent, aDOMEvent, aEventStatus);
  }

  //Bubbling stage
  if ((DOM_EVENT_CAPTURE != aFlags) && (mParent != nsnull)) {
    ret = mParent->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                  DOM_EVENT_BUBBLE, aEventStatus);
  }

  if (DOM_EVENT_INIT == aFlags) {
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
  // lazy allocation of range list
  if (nsnull == mRangeList) {
    mRangeList = new nsVoidArray();
  }
  if (nsnull == mRangeList) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  // dont need to addref - this call is made by the range object itself
  PRBool rv = mRangeList->AppendElement(&aRange);
  if (rv)  return NS_OK;
  return NS_ERROR_FAILURE;
}


nsresult 
nsGenericElement::RangeRemove(nsIDOMRange& aRange)
{
  if (mRangeList) {
    // dont need to release - this call is made by the range object itself
    PRBool rv = mRangeList->RemoveElement(&aRange);
    if (rv)  return NS_OK;
  }
  return NS_ERROR_FAILURE;
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
    frame = shell->FindFrameWithContent(mContent);
    while (nsnull != frame) {
      nsIViewManager* vm;
      nsIView* view;

      // Determine damaged area and tell view manager to redraw it
      frame->GetRect(bounds);
      bounds.x = bounds.y = 0;

      // XXX We should tell the frame the damage area and let it invalidate
      // itself. Add some API calls to nsIFrame to allow a caller to invalidate
      // parts of the frame...
      frame->GetOffsetFromView(offset, view);
      view->GetViewManager(vm);
      bounds.x += offset.x;
      bounds.y += offset.y;

      vm->UpdateView(view, bounds, NS_VMREFRESH_IMMEDIATE);
      NS_RELEASE(vm);

      // If frame has a next-in-flow, repaint it too
      frame->GetNextInFlow(frame);
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
nsGenericElement::AddEventListener(nsIDOMEventListener* aListener,
                                   const nsIID& aIID)
{
  nsIEventListenerManager *manager;

  if (NS_OK == GetListenerManager(&manager)) {
    manager->AddEventListener(aListener, aIID);
    NS_RELEASE(manager);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsresult
nsGenericElement::RemoveEventListener(nsIDOMEventListener* aListener,
                                      const nsIID& aIID)
{
  if (nsnull != mListenerManager) {
    mListenerManager->RemoveEventListener(aListener, aIID);
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
    prefix.SetString(propName, 2);
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
                              const nsString& aBase,
                              const nsString& aURLSpec,
                              const nsString& aTargetSpec,
                              PRBool aClick)
{
  nsILinkHandler* handler;
  nsresult rv = aPresContext.GetLinkHandler(&handler);
  if (NS_SUCCEEDED(rv) && (nsnull != handler)) {
    // Resolve url to an absolute url
    nsIURL* docURL = nsnull;
    nsIDocument* doc;
    rv = GetDocument(doc);
    if (NS_SUCCEEDED(rv) && (nsnull != doc)) {
      docURL = doc->GetDocumentURL();
      NS_RELEASE(doc);
    }

    nsAutoString absURLSpec;
    if (aURLSpec.Length() > 0) {
      nsresult rv = NS_MakeAbsoluteURL(docURL, aBase, aURLSpec, absURLSpec);
    }
    else {
      absURLSpec = aURLSpec;
    }

    NS_IF_RELEASE(docURL);

    // Now pass on absolute url to the click handler
    if (aClick) {
      handler->OnLinkClick(mContent, aVerb, absURLSpec, aTargetSpec);
    }
    else {
      handler->OnOverLink(mContent, absURLSpec, aTargetSpec);
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

//----------------------------------------------------------------------

// Nothing for now, though XMLContent might want to define this
// to deal with certain types of attributes with the HTML namespace
static void
MapAttributesInto(nsIHTMLAttributes* aAttributes,
                  nsIStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
}

// XXX This routine was copied from EnsureWriteableAttributes and
// modified slightly. Could be shared code.
static nsresult EnsureWritableAttributes(nsIContent* aContent,
                                         nsIHTMLAttributes*& aAttributes, 
                                         PRBool aCreate)
{
  nsresult  result = NS_OK;

  if (nsnull == aAttributes) {
    if (PR_TRUE == aCreate) {
      if (NS_OK == result) {
        result = NS_NewHTMLAttributes(&aAttributes, nsnull, &MapAttributesInto);
        if (NS_OK == result) {
          aAttributes->AddContentRef();
        }
      }
    }
  }
  else {
    // To allow for the case where the attribute set is shared
    // between content.
    PRInt32 contentRefCount;
    aAttributes->GetContentRefCount(contentRefCount);
    if (1 < contentRefCount) {
      nsIHTMLAttributes*  attrs;
      result = aAttributes->Clone(&attrs);
      if (NS_OK == result) {
        aAttributes->ReleaseContentRef();
        NS_RELEASE(aAttributes);
        aAttributes = attrs;
        aAttributes->AddContentRef();
      }
    }
  }
  return result;
}

static void ReleaseAttributes(nsIHTMLAttributes*& aAttributes)
{
  aAttributes->ReleaseContentRef();
  NS_RELEASE(aAttributes);
}

nsGenericContainerElement::nsGenericContainerElement()
{
  mAttributes = nsnull;
}

nsGenericContainerElement::~nsGenericContainerElement()
{
  PRInt32 n = mChildren.Count();
  for (PRInt32 i = 0; i < n; i++) {
    nsIContent* kid = (nsIContent *)mChildren.ElementAt(i);
    NS_RELEASE(kid);
  }
  if (nsnull != mAttributes) {
    ReleaseAttributes(mAttributes);
  }
}

nsresult
nsGenericContainerElement::CopyInnerTo(nsIContent* aSrcContent,
                                       nsGenericContainerElement* aDst)
{
  aDst->mContent = aSrcContent;
  // XXX should the node's document be set?
  // XXX copy attributes not yet impelemented
  // XXX deep copy?
  return NS_OK;
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

// XXX It's possible that newChild has already been inserted in the
// tree; if this is the case then we need to remove it from where it
// was before placing it in it's new home

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
          SetDocumentInChildrenOf(newContent, mDocument);
          res = ReplaceChildAt(newContent, pos, PR_TRUE);
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
nsGenericContainerElement::SetAttribute(const nsString& aName, 
                                        const nsString& aValue,
                                        PRBool aNotify)
{
  nsresult rv = NS_OK;;

  nsIAtom* attr = NS_NewAtom(aName);
  rv = EnsureWritableAttributes(mContent, mAttributes, PR_TRUE);
  if (NS_SUCCEEDED(rv)) {
    PRInt32 count;
    rv = mAttributes->SetAttribute(attr, aValue, count);
    if (0 == count) {
      ReleaseAttributes(mAttributes);
    }
    if (NS_SUCCEEDED(rv) && aNotify && (nsnull != mDocument)) {
      mDocument->AttributeChanged(mContent, attr, NS_STYLE_HINT_UNKNOWN);
    }
  }
  NS_RELEASE(attr);

  return rv;
}

nsresult 
nsGenericContainerElement::GetAttribute(const nsString& aName, 
                                        nsString& aResult) const
{
  nsresult rv = NS_OK;;

  nsIAtom* attr = NS_NewAtom(aName);
  nsHTMLValue value;
  char cbuf[20];
  nscolor color;
  if (nsnull != mAttributes) {
    rv = mAttributes->GetAttribute(attr, value);
    if (NS_CONTENT_ATTR_HAS_VALUE == rv) {
      // XXX Again this is code that is copied from nsGenericHTMLElement
      // and could potentially be shared. In any case, we don't expect
      // anything that's not a string.
      // Provide default conversions for most everything
      switch (value.GetUnit()) {
        case eHTMLUnit_Empty:
          aResult.Truncate();
          break;

        case eHTMLUnit_String:
        case eHTMLUnit_Null:
          value.GetStringValue(aResult);
          break;

        case eHTMLUnit_Integer:
          aResult.Truncate();
          aResult.Append(value.GetIntValue(), 10);
          break;

        case eHTMLUnit_Pixel:
          aResult.Truncate();
          aResult.Append(value.GetPixelValue(), 10);
          break;

        case eHTMLUnit_Percent:
          aResult.Truncate(0);
          aResult.Append(PRInt32(value.GetPercentValue() * 100.0f), 10);
          aResult.Append('%');
          break;

        case eHTMLUnit_Color:
          color = nscolor(value.GetColorValue());
          PR_snprintf(cbuf, sizeof(cbuf), "#%02x%02x%02x",
                      NS_GET_R(color), NS_GET_G(color), NS_GET_B(color));
          aResult.Truncate(0);
          aResult.Append(cbuf);
          break;
          
        default:
        case eHTMLUnit_Enumerated:
          NS_NOTREACHED("no default enumerated value to string conversion");
          rv = NS_CONTENT_ATTR_NOT_THERE;
          break;
      }
    }
  }
  else {
    rv = NS_CONTENT_ATTR_NOT_THERE;
  }
  NS_RELEASE(attr);
  
  return rv;
}

nsresult 
nsGenericContainerElement::UnsetAttribute(nsIAtom* aAttribute, PRBool aNotify)
{
  nsresult rv = NS_OK;;

  rv = EnsureWritableAttributes(mContent, mAttributes, PR_FALSE);
  if (nsnull != mAttributes) {
    PRInt32 count;
    rv = mAttributes->UnsetAttribute(aAttribute, count);
    if (0 == count) {
      ReleaseAttributes(mAttributes);
    }

    if (NS_SUCCEEDED(rv) && aNotify && (nsnull != mDocument)) {
      mDocument->AttributeChanged(mContent, aAttribute, NS_STYLE_HINT_UNKNOWN);
    }
  }

  return rv;
}

nsresult 
nsGenericContainerElement::GetAllAttributeNames(nsISupportsArray* aArray,
                                                PRInt32& aCount) const
{
  if (nsnull != mAttributes) {
    return mAttributes->GetAllAttributeNames(aArray, aCount);
  }
  aCount = 0;
  return NS_OK;
}

nsresult 
nsGenericContainerElement::GetAttributeCount(PRInt32& aResult) const
{
  if (nsnull != mAttributes) {
    return mAttributes->Count(aResult);
  }
  aResult = 0;
  return NS_OK;
}

void
nsGenericContainerElement::ListAttributes(FILE* out) const
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
nsGenericContainerElement::SizeOf(nsISizeOfHandler* aHandler) const
{
  aHandler->Add(sizeof(*this));
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
    PRBool rv = mChildren.RemoveElementAt(aIndex);
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
