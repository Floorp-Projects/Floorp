/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Base class for all element classes; this provides an implementation
 * of DOM Core's nsIDOMElement, implements nsIContent, provides
 * utility methods for subclasses, and so forth.
 */

#include "nsGenericElement.h"

#include "nsDOMAttribute.h"
#include "nsDOMAttributeMap.h"
#include "nsIAtom.h"
#include "nsINodeInfo.h"
#include "nsIDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMText.h"
#include "nsIDOMEventReceiver.h"
#include "nsIContentIterator.h"
#include "nsIEventListenerManager.h"
#include "nsIFocusController.h"
#include "nsILinkHandler.h"
#include "nsIScriptGlobalObject.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsStyleConsts.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "nsIEventStateManager.h"
#include "nsIDOMEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsDOMCSSDeclaration.h"
#include "nsINameSpaceManager.h"
#include "nsContentList.h"
#include "nsDOMError.h"
#include "nsDOMString.h"
#include "nsIScriptSecurityManager.h"
#include "nsIDOMMutationEvent.h"
#include "nsMutationEvent.h"
#include "nsNodeUtils.h"
#include "nsDocument.h"

#include "nsBindingManager.h"
#include "nsXBLBinding.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMViewCSS.h"
#include "nsIXBLService.h"
#include "nsPIDOMWindow.h"
#include "nsIBoxObject.h"
#include "nsPIBoxObject.h"
#include "nsIDOMNSDocument.h"
#include "nsIDOMNSElement.h"

#include "nsGkAtoms.h"
#include "nsContentUtils.h"
#include "nsIJSContextStack.h"

#include "nsIServiceManager.h"
#include "nsIDOMEventListener.h"

#include "nsIWebNavigation.h"
#include "nsIBaseWindow.h"

#include "jsapi.h"

#include "nsNodeInfoManager.h"
#include "nsICategoryManager.h"
#include "nsIDOMNSFeatureFactory.h"
#include "nsIDOMDocumentType.h"
#include "nsIDOMUserDataHandler.h"
#include "nsEventDispatcher.h"
#include "nsContentCreatorFunctions.h"
#include "nsIFocusController.h"
#include "nsIControllers.h"


#include "nsCycleCollectionParticipant.h"

#ifdef MOZ_SVG
PRBool NS_SVG_TestFeature(const nsAString &fstr);
#endif /* MOZ_SVG */

#ifdef DEBUG_waterson

/**
 * List a content tree to stdout. Meant to be called from gdb.
 */
void
DebugListContentTree(nsIContent* aElement)
{
  aElement->List(stdout, 0);
  printf("\n");
}

#endif

PRInt32 nsIContent::sTabFocusModel = eTabFocus_any;
PRBool nsIContent::sTabFocusModelAppliesToXUL = PR_FALSE;
nsresult NS_NewContentIterator(nsIContentIterator** aInstancePtrResult);

//----------------------------------------------------------------------

nsINode::nsSlots::~nsSlots()
{
  if (mChildNodes) {
    mChildNodes->DropReference();
    NS_RELEASE(mChildNodes);
  }

  if (mWeakReference) {
    mWeakReference->NoticeNodeDestruction();
  }
}

//----------------------------------------------------------------------

nsINode::~nsINode()
{
  NS_ASSERTION(!HasSlots(), "nsNodeUtils::LastRelease was not called?");
}

void*
nsINode::GetProperty(PRUint16 aCategory, nsIAtom *aPropertyName,
                     nsresult *aStatus) const
{
  nsIDocument *doc = GetOwnerDoc();
  if (!doc)
    return nsnull;

  return doc->PropertyTable()->GetProperty(this, aCategory, aPropertyName,
                                           aStatus);
}

nsresult
nsINode::SetProperty(PRUint16 aCategory, nsIAtom *aPropertyName, void *aValue,
                     NSPropertyDtorFunc aDtor, PRBool aTransfer,
                     void **aOldValue)
{
  nsIDocument *doc = GetOwnerDoc();
  if (!doc)
    return NS_ERROR_FAILURE;

  nsresult rv = doc->PropertyTable()->SetProperty(this, aCategory,
                                                  aPropertyName, aValue, aDtor,
                                                  nsnull, aTransfer, aOldValue);
  if (NS_SUCCEEDED(rv)) {
    SetFlags(NODE_HAS_PROPERTIES);
  }

  return rv;
}

nsresult
nsINode::DeleteProperty(PRUint16 aCategory, nsIAtom *aPropertyName)
{
  nsIDocument *doc = GetOwnerDoc();
  if (!doc)
    return nsnull;

  return doc->PropertyTable()->DeleteProperty(this, aCategory, aPropertyName);
}

void*
nsINode::UnsetProperty(PRUint16 aCategory, nsIAtom *aPropertyName,
                       nsresult *aStatus)
{
  nsIDocument *doc = GetOwnerDoc();
  if (!doc)
    return nsnull;

  return doc->PropertyTable()->UnsetProperty(this, aCategory, aPropertyName,
                                             aStatus);
}

NS_IMETHODIMP
nsGenericElement::GetListenerManager(PRBool aCreateIfNotFound,
                                     nsIEventListenerManager** aResult)
{
  return nsContentUtils::GetListenerManager(this, aCreateIfNotFound, aResult);
}

nsINode::nsSlots*
nsINode::CreateSlots()
{
  return new nsSlots(mFlagsOrSlots);
}

void
nsINode::AddMutationObserver(nsIMutationObserver* aMutationObserver)
{
  nsSlots* slots = GetSlots();
  if (slots) {
    slots->mMutationObservers.AppendObserver(aMutationObserver);
  }
}

void
nsINode::RemoveMutationObserver(nsIMutationObserver* aMutationObserver)
{
  nsSlots* slots = GetExistingSlots();
  if (slots) {
    slots->mMutationObservers.RemoveObserver(aMutationObserver);
  }
}

//----------------------------------------------------------------------

void
nsIContent::SetNativeAnonymous(PRBool aAnonymous)
{
  if (aAnonymous) {
    SetFlags(NODE_IS_ANONYMOUS);
    SetFlags(NODE_IS_ANONYMOUS_FOR_EVENTS);
  } else {
    UnsetFlags(NODE_IS_ANONYMOUS);
    UnsetFlags(NODE_IS_ANONYMOUS_FOR_EVENTS);
  }
}

//----------------------------------------------------------------------

nsChildContentList::~nsChildContentList()
{
  MOZ_COUNT_DTOR(nsChildContentList);
}

NS_IMETHODIMP
nsChildContentList::GetLength(PRUint32* aLength)
{
  *aLength = mNode ? mNode->GetChildCount() : 0;

  return NS_OK;
}

NS_IMETHODIMP
nsChildContentList::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  if (mNode) {
    nsIContent *content = mNode->GetChildAt(aIndex);
    if (content) {
      return CallQueryInterface(content, aReturn);
    }
  }

  *aReturn = nsnull;

  return NS_OK;
}

//----------------------------------------------------------------------

NS_IMPL_CYCLE_COLLECTION_1(nsNode3Tearoff, mContent)

NS_INTERFACE_MAP_BEGIN(nsNode3Tearoff)
  NS_INTERFACE_MAP_ENTRY(nsIDOM3Node)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsNode3Tearoff)
NS_INTERFACE_MAP_END_AGGREGATED(mContent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsNode3Tearoff)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsNode3Tearoff)

NS_IMETHODIMP
nsNode3Tearoff::GetBaseURI(nsAString& aURI)
{
  nsCOMPtr<nsIURI> baseURI = mContent->GetBaseURI();
  nsCAutoString spec;

  if (baseURI) {
    baseURI->GetSpec(spec);
  }

  CopyUTF8toUTF16(spec, aURI);
  
  return NS_OK;
}

NS_IMETHODIMP
nsNode3Tearoff::GetTextContent(nsAString &aTextContent)
{
  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mContent));
  NS_ASSERTION(node, "We have an nsIContent which doesn't support nsIDOMNode");

  PRUint16 nodeType;
  node->GetNodeType(&nodeType);
  if (nodeType == nsIDOMNode::DOCUMENT_TYPE_NODE ||
      nodeType == nsIDOMNode::NOTATION_NODE) {
    SetDOMStringToNull(aTextContent);

    return NS_OK;
  }

  if (nodeType == nsIDOMNode::TEXT_NODE ||
      nodeType == nsIDOMNode::CDATA_SECTION_NODE ||
      nodeType == nsIDOMNode::COMMENT_NODE ||
      nodeType == nsIDOMNode::PROCESSING_INSTRUCTION_NODE) {
    return node->GetNodeValue(aTextContent);
  }

  nsContentUtils::GetNodeTextContent(mContent, PR_TRUE, aTextContent);

  return NS_OK;
}

NS_IMETHODIMP
nsNode3Tearoff::SetTextContent(const nsAString &aTextContent)
{
  // Batch possible DOMSubtreeModified events.
  mozAutoSubtreeModified subtree(mContent->GetOwnerDoc(), nsnull);

  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mContent));
  NS_ASSERTION(node, "We have an nsIContent which doesn't support nsIDOMNode");

  PRUint16 nodeType;
  node->GetNodeType(&nodeType);
  if (nodeType == nsIDOMNode::DOCUMENT_TYPE_NODE ||
      nodeType == nsIDOMNode::NOTATION_NODE) {
    return NS_OK;
  }

  if (nodeType == nsIDOMNode::TEXT_NODE ||
      nodeType == nsIDOMNode::CDATA_SECTION_NODE ||
      nodeType == nsIDOMNode::COMMENT_NODE ||
      nodeType == nsIDOMNode::PROCESSING_INSTRUCTION_NODE) {
    return node->SetNodeValue(aTextContent);
  }

  return nsContentUtils::SetNodeTextContent(mContent, aTextContent, PR_FALSE);
}

NS_IMETHODIMP
nsNode3Tearoff::CompareDocumentPosition(nsIDOMNode* aOther,
                                        PRUint16* aReturn)
{
  NS_ENSURE_ARG_POINTER(aOther);

  nsCOMPtr<nsINode> other = do_QueryInterface(aOther);
  NS_ENSURE_TRUE(other, NS_ERROR_DOM_NOT_SUPPORTED_ERR);

  *aReturn = nsContentUtils::ComparePosition(other, mContent);
  return NS_OK;
}

NS_IMETHODIMP
nsNode3Tearoff::IsSameNode(nsIDOMNode* aOther,
                           PRBool* aReturn)
{
  nsCOMPtr<nsIContent> other(do_QueryInterface(aOther));
  *aReturn = mContent == other;

  return NS_OK;
}

PRBool
nsNode3Tearoff::AreNodesEqual(nsIContent* aContent1,
                              nsIContent* aContent2)
{
  // We use nsIContent instead of nsINode for the attributes of elements.

  NS_PRECONDITION(aContent1 && aContent2, "Who called AreNodesEqual?");

  nsAutoString string1, string2;

  // Prefix, namespace URI, local name, node name check.
  if (!aContent1->NodeInfo()->Equals(aContent2->NodeInfo())) {
    return PR_FALSE;
  }

  if (aContent1->Tag() == nsGkAtoms::documentTypeNodeName) {
    nsCOMPtr<nsIDOMDocumentType> docType1 = do_QueryInterface(aContent1);
    nsCOMPtr<nsIDOMDocumentType> docType2 = do_QueryInterface(aContent2);

    NS_ASSERTION(docType1 && docType2, "Why don't we have a document type node?");

    // Public ID
    docType1->GetPublicId(string1);
    docType2->GetPublicId(string2);

    if (!string1.Equals(string2)) {
      return PR_FALSE;
    }

    // System ID
    docType1->GetSystemId(string1);
    docType2->GetSystemId(string2);

    if (!string1.Equals(string2)) {
      return PR_FALSE;
    }

    // Internal subset
    docType1->GetInternalSubset(string1);
    docType2->GetInternalSubset(string2);

    if (!string1.Equals(string2)) {
      return PR_FALSE;
    }
  }

  if (aContent1->IsNodeOfType(nsINode::eELEMENT)) {
    // aContent1 is an element.  Do the check on attributes.
    PRUint32 attrCount = aContent1->GetAttrCount();
    if (attrCount != aContent2->GetAttrCount()) {
      return PR_FALSE;
    }

    // Iterate over attributes.
    for (PRUint32 i = 0; i < attrCount; ++i) {
      const nsAttrName* attrName1 = aContent1->GetAttrNameAt(i);
#ifdef DEBUG
      PRBool hasAttr =
#endif
      aContent1->GetAttr(attrName1->NamespaceID(),
                         attrName1->LocalName(),
                         string1);
      NS_ASSERTION(hasAttr, "Why don't we have an attr?");

      if (!aContent2->AttrValueIs(attrName1->NamespaceID(),
                                  attrName1->LocalName(),
                                  string1,
                                  eCaseMatters)) {
        return PR_FALSE;
      }
    }
  } else {
    // aContent1 is not an element.  Node value check.
    nsCOMPtr<nsIDOMNode> domNode1 = do_QueryInterface(aContent1);
    nsCOMPtr<nsIDOMNode> domNode2 = do_QueryInterface(aContent2);
    NS_ASSERTION(domNode1 && domNode2, "How'd we get nsIContent without nsIDOMNode?");
    domNode1->GetNodeValue(string1);
    domNode2->GetNodeValue(string2);
    if (!string1.Equals(string2)) {
      return PR_FALSE;
    }
  }

  // Child nodes count.
  PRUint32 childCount = aContent1->GetChildCount();
  if (childCount != aContent2->GetChildCount()) {
    return PR_FALSE;
  }

  // Iterate over child nodes.
  for (PRUint32 i = 0; i < childCount; ++i) {
    nsIContent* child1 = aContent1->GetChildAt(i);
    nsIContent* child2 = aContent2->GetChildAt(i);
    if (!AreNodesEqual(child1, child2)) {
      return PR_FALSE;
    }
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsNode3Tearoff::IsEqualNode(nsIDOMNode* aOther, PRBool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aOther);

  *aReturn = PR_FALSE;

  // Since we implement nsIContent, aOther must as well.
  nsCOMPtr<nsIContent> aOtherContent = do_QueryInterface(aOther);
  // Documents and attributes don't implement nsIContent.
  if (!aOtherContent) {
    return NS_OK;
  }

  *aReturn = nsNode3Tearoff::AreNodesEqual(mContent, aOtherContent);
  return NS_OK;
}

NS_IMETHODIMP
nsNode3Tearoff::GetFeature(const nsAString& aFeature,
                           const nsAString& aVersion,
                           nsISupports** aReturn)
{
  return nsGenericElement::InternalGetFeature(this, aFeature, aVersion, aReturn);
}

NS_IMETHODIMP
nsNode3Tearoff::SetUserData(const nsAString& aKey,
                            nsIVariant* aData,
                            nsIDOMUserDataHandler* aHandler,
                            nsIVariant** aResult)
{
  nsCOMPtr<nsIAtom> key = do_GetAtom(aKey);
  if (!key) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return nsContentUtils::SetUserData(mContent, key, aData, aHandler, aResult);
}

NS_IMETHODIMP
nsNode3Tearoff::GetUserData(const nsAString& aKey,
                            nsIVariant** aResult)
{
  nsIDocument *document = mContent->GetOwnerDoc();
  NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);

  nsCOMPtr<nsIAtom> key = do_GetAtom(aKey);
  if (!key) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aResult = NS_STATIC_CAST(nsIVariant*,
                            mContent->GetProperty(DOM_USER_DATA, key));
  NS_IF_ADDREF(*aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsNode3Tearoff::LookupPrefix(const nsAString& aNamespaceURI,
                             nsAString& aPrefix)
{
  SetDOMStringToNull(aPrefix);

  // XXX Waiting for DOM spec to list error codes.

  // Trace up the content parent chain looking for the namespace
  // declaration that defines the aNamespaceURI namespace. Once found,
  // return the prefix (i.e. the attribute localName).
  for (nsIContent* content = mContent; content;
       content = content->GetParent()) {
    PRUint32 attrCount = content->GetAttrCount();

    for (PRUint32 i = 0; i < attrCount; ++i) {
      const nsAttrName* name = content->GetAttrNameAt(i);

      if (name->NamespaceEquals(kNameSpaceID_XMLNS) &&
          content->AttrValueIs(kNameSpaceID_XMLNS, name->LocalName(),
                               aNamespaceURI, eCaseMatters)) {
        // If the localName is "xmlns", the prefix we output should be
        // null.
        if (name->LocalName() != nsGkAtoms::xmlns) {
          name->LocalName()->ToString(aPrefix);
        }

        return NS_OK;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNode3Tearoff::LookupNamespaceURI(const nsAString& aNamespacePrefix,
                                   nsAString& aNamespaceURI)
{
  if (NS_FAILED(nsContentUtils::LookupNamespaceURI(mContent,
                                                   aNamespacePrefix,
                                                   aNamespaceURI))) {
    SetDOMStringToNull(aNamespaceURI);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNode3Tearoff::IsDefaultNamespace(const nsAString& aNamespaceURI,
                                   PRBool* aReturn)
{
  nsAutoString defaultNamespace;
  LookupNamespaceURI(EmptyString(), defaultNamespace);
  *aReturn = aNamespaceURI.Equals(defaultNamespace);
  return NS_OK;
}

//----------------------------------------------------------------------


NS_INTERFACE_MAP_BEGIN(nsNSElementTearoff)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSElement)
NS_INTERFACE_MAP_END_AGGREGATED(mContent)

NS_IMPL_ADDREF(nsNSElementTearoff)
NS_IMPL_RELEASE(nsNSElementTearoff)

NS_IMETHODIMP
nsNSElementTearoff::GetElementsByClassName(const nsAString& aClasses,
                                           nsIDOMNodeList** aReturn)
{
  return nsDocument::GetElementsByClassNameHelper(mContent, aClasses, aReturn);
}

//----------------------------------------------------------------------


NS_IMPL_ISUPPORTS1(nsNodeWeakReference,
                   nsIWeakReference)

nsNodeWeakReference::~nsNodeWeakReference()
{
  if (mNode) {
    NS_ASSERTION(mNode->GetSlots() &&
                 mNode->GetSlots()->mWeakReference == this,
                 "Weak reference has wrong value");
    mNode->GetSlots()->mWeakReference = nsnull;
  }
}

NS_IMETHODIMP
nsNodeWeakReference::QueryReferent(const nsIID& aIID, void** aInstancePtr)
{
  return mNode ? mNode->QueryInterface(aIID, aInstancePtr) :
                 NS_ERROR_NULL_POINTER;
}


NS_IMPL_CYCLE_COLLECTION_1(nsNodeSupportsWeakRefTearoff, mNode)

NS_INTERFACE_MAP_BEGIN(nsNodeSupportsWeakRefTearoff)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsNodeSupportsWeakRefTearoff)
NS_INTERFACE_MAP_END_AGGREGATED(mNode)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsNodeSupportsWeakRefTearoff)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsNodeSupportsWeakRefTearoff)

NS_IMETHODIMP
nsNodeSupportsWeakRefTearoff::GetWeakReference(nsIWeakReference** aInstancePtr)
{
  nsINode::nsSlots* slots = mNode->GetSlots();
  NS_ENSURE_TRUE(slots, NS_ERROR_OUT_OF_MEMORY);

  if (!slots->mWeakReference) {
    slots->mWeakReference = new nsNodeWeakReference(mNode);
    NS_ENSURE_TRUE(slots->mWeakReference, NS_ERROR_OUT_OF_MEMORY);
  }

  NS_ADDREF(*aInstancePtr = slots->mWeakReference);

  return NS_OK;
}

//----------------------------------------------------------------------

nsDOMEventRTTearoff *
nsDOMEventRTTearoff::mCachedEventTearoff[NS_EVENT_TEAROFF_CACHE_SIZE];

PRUint32 nsDOMEventRTTearoff::mCachedEventTearoffCount = 0;


nsDOMEventRTTearoff::nsDOMEventRTTearoff(nsIContent *aContent)
  : mContent(aContent)
{
}

nsDOMEventRTTearoff::~nsDOMEventRTTearoff()
{
}

NS_IMPL_CYCLE_COLLECTION_1(nsDOMEventRTTearoff, mContent)

NS_INTERFACE_MAP_BEGIN(nsDOMEventRTTearoff)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventReceiver)
  NS_INTERFACE_MAP_ENTRY(nsIDOM3EventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSEventTarget)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsDOMEventRTTearoff)
NS_INTERFACE_MAP_END_AGGREGATED(mContent)

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsDOMEventRTTearoff,
                                          nsIDOMEventTarget)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS_WITH_DESTROY(nsDOMEventRTTearoff,
                                                        nsIDOMEventTarget,
                                                        LastRelease())

nsDOMEventRTTearoff *
nsDOMEventRTTearoff::Create(nsIContent *aContent)
{
  if (mCachedEventTearoffCount) {
    // We have cached unused instances of this class, return a cached
    // instance in stead of always creating a new one.
    nsDOMEventRTTearoff *tearoff =
      mCachedEventTearoff[--mCachedEventTearoffCount];

    // Set the back pointer to the content object
    tearoff->mContent = aContent;

    return tearoff;
  }

  // The cache is empty, this means we haveto create a new instance.
  return new nsDOMEventRTTearoff(aContent);
}

// static
void
nsDOMEventRTTearoff::Shutdown()
{
  // Clear our cache.
  while (mCachedEventTearoffCount) {
    delete mCachedEventTearoff[--mCachedEventTearoffCount];
  }
}

void
nsDOMEventRTTearoff::LastRelease()
{
  if (mCachedEventTearoffCount < NS_EVENT_TEAROFF_CACHE_SIZE) {
    // There's still space in the cache for one more instance, put
    // this instance in the cache in stead of deleting it.
    mCachedEventTearoff[mCachedEventTearoffCount++] = this;

    // Don't set mContent to null directly since setting mContent to null
    // could result in code that grabs a tearoff from the cache and we don't
    // want to get reused while still being torn down.
    // See bug 330526.
    nsCOMPtr<nsIContent> kungFuDeathGrip;
    kungFuDeathGrip.swap(mContent);

    // The refcount balancing and destructor re-entrancy protection
    // code in Release() sets mRefCnt to 1 so we have to set it to 0
    // here to prevent leaks
    mRefCnt = 0;

    return;
  }

  delete this;
}

nsresult
nsDOMEventRTTearoff::GetEventReceiver(nsIDOMEventReceiver **aReceiver)
{
  nsCOMPtr<nsIEventListenerManager> listener_manager;
  nsresult rv =
    mContent->GetListenerManager(PR_TRUE, getter_AddRefs(listener_manager));
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(listener_manager, aReceiver);
}

nsresult
nsDOMEventRTTearoff::GetDOM3EventTarget(nsIDOM3EventTarget **aTarget)
{
  nsCOMPtr<nsIEventListenerManager> listener_manager;
  nsresult rv =
    mContent->GetListenerManager(PR_TRUE, getter_AddRefs(listener_manager));
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(listener_manager, aTarget);
}

NS_IMETHODIMP
nsDOMEventRTTearoff::AddEventListenerByIID(nsIDOMEventListener *aListener,
                                           const nsIID& aIID)
{
  nsCOMPtr<nsIDOMEventReceiver> event_receiver;
  nsresult rv = GetEventReceiver(getter_AddRefs(event_receiver));
  NS_ENSURE_SUCCESS(rv, rv);

  return event_receiver->AddEventListenerByIID(aListener, aIID);
}

NS_IMETHODIMP
nsDOMEventRTTearoff::RemoveEventListenerByIID(nsIDOMEventListener *aListener,
                                              const nsIID& aIID)
{
  nsCOMPtr<nsIDOMEventReceiver> event_receiver;
  nsresult rv = GetEventReceiver(getter_AddRefs(event_receiver));
  NS_ENSURE_SUCCESS(rv, rv);

  return event_receiver->RemoveEventListenerByIID(aListener, aIID);
}

NS_IMETHODIMP
nsDOMEventRTTearoff::GetListenerManager(PRBool aCreateIfNotFound,
                                        nsIEventListenerManager** aResult)
{
  return mContent->GetListenerManager(aCreateIfNotFound, aResult);
}

NS_IMETHODIMP
nsDOMEventRTTearoff::HandleEvent(nsIDOMEvent *aEvent)
{
  nsCOMPtr<nsIDOMEventReceiver> event_receiver;
  nsresult rv = GetEventReceiver(getter_AddRefs(event_receiver));
  NS_ENSURE_SUCCESS(rv, rv);

  return event_receiver->HandleEvent(aEvent);
}

NS_IMETHODIMP
nsDOMEventRTTearoff::GetSystemEventGroup(nsIDOMEventGroup **aGroup)
{
  nsCOMPtr<nsIEventListenerManager> manager;
  GetListenerManager(PR_TRUE, getter_AddRefs(manager));

  if (!manager) {
    return NS_ERROR_FAILURE;
  }

  return manager->GetSystemEventGroupLM(aGroup);
}

NS_IMETHODIMP
nsDOMEventRTTearoff::GetScriptTypeID(PRUint32 *aLang)
{
    *aLang = mContent->GetScriptTypeID();
    return NS_OK;
}

NS_IMETHODIMP
nsDOMEventRTTearoff::SetScriptTypeID(PRUint32 aLang)
{
    return mContent->SetScriptTypeID(aLang);
}


// nsIDOMEventTarget
NS_IMETHODIMP
nsDOMEventRTTearoff::AddEventListener(const nsAString& aType,
                                      nsIDOMEventListener *aListener,
                                      PRBool useCapture)
{
  return
    AddEventListener(aType, aListener, useCapture,
                     !nsContentUtils::IsChromeDoc(mContent->GetOwnerDoc()));
}

NS_IMETHODIMP
nsDOMEventRTTearoff::RemoveEventListener(const nsAString& type,
                                         nsIDOMEventListener *listener,
                                         PRBool useCapture)
{
  nsCOMPtr<nsIDOMEventReceiver> event_receiver;
  nsresult rv = GetEventReceiver(getter_AddRefs(event_receiver));
  NS_ENSURE_SUCCESS(rv, rv);

  return event_receiver->RemoveEventListener(type, listener, useCapture);
}

NS_IMETHODIMP
nsDOMEventRTTearoff::DispatchEvent(nsIDOMEvent *evt, PRBool* _retval)
{
  nsCOMPtr<nsIDOMEventReceiver> event_receiver;
  nsresult rv = GetEventReceiver(getter_AddRefs(event_receiver));
  NS_ENSURE_SUCCESS(rv, rv);

  return event_receiver->DispatchEvent(evt, _retval);
}

// nsIDOM3EventTarget
NS_IMETHODIMP
nsDOMEventRTTearoff::AddGroupedEventListener(const nsAString& aType,
                                             nsIDOMEventListener *aListener,
                                             PRBool aUseCapture,
                                             nsIDOMEventGroup *aEvtGrp)
{
  nsCOMPtr<nsIDOM3EventTarget> event_target;
  nsresult rv = GetDOM3EventTarget(getter_AddRefs(event_target));
  NS_ENSURE_SUCCESS(rv, rv);

  return event_target->AddGroupedEventListener(aType, aListener, aUseCapture,
                                               aEvtGrp);
}

NS_IMETHODIMP
nsDOMEventRTTearoff::RemoveGroupedEventListener(const nsAString& aType,
                                                nsIDOMEventListener *aListener,
                                                PRBool aUseCapture,
                                                nsIDOMEventGroup *aEvtGrp)
{
  nsCOMPtr<nsIDOM3EventTarget> event_target;
  nsresult rv = GetDOM3EventTarget(getter_AddRefs(event_target));
  NS_ENSURE_SUCCESS(rv, rv);

  return event_target->RemoveGroupedEventListener(aType, aListener,
                                                  aUseCapture, aEvtGrp);
}

NS_IMETHODIMP
nsDOMEventRTTearoff::CanTrigger(const nsAString & type, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMEventRTTearoff::IsRegisteredHere(const nsAString & type, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIDOMNSEventTarget
NS_IMETHODIMP
nsDOMEventRTTearoff::AddEventListener(const nsAString& aType,
                                      nsIDOMEventListener *aListener,
                                      PRBool aUseCapture,
                                      PRBool aWantsUntrusted)
{
  nsCOMPtr<nsIEventListenerManager> listener_manager;
  nsresult rv =
    mContent->GetListenerManager(PR_TRUE, getter_AddRefs(listener_manager));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

  if (aWantsUntrusted) {
    flags |= NS_PRIV_EVENT_UNTRUSTED_PERMITTED;
  }

  return listener_manager->AddEventListenerByType(aListener, aType, flags,
                                                  nsnull);
}

//----------------------------------------------------------------------

PRUint32 nsMutationGuard::sMutationCount = 0;

nsGenericElement::nsDOMSlots::nsDOMSlots(PtrBits aFlags)
  : nsINode::nsSlots(aFlags),
    mBindingParent(nsnull)
{
}

nsGenericElement::nsDOMSlots::~nsDOMSlots()
{
  if (mStyle) {
    mStyle->DropReference();
  }

  if (mAttributeMap) {
    mAttributeMap->DropReference();
  }
}

nsGenericElement::nsGenericElement(nsINodeInfo *aNodeInfo)
  : nsIContent(aNodeInfo)
{
  // Set the default scriptID to JS - but skip SetScriptTypeID as it
  // does extra work we know isn't necessary here...
  SetFlags(nsIProgrammingLanguage::JAVASCRIPT << NODE_SCRIPT_TYPE_OFFSET);
}

nsGenericElement::~nsGenericElement()
{
  NS_PRECONDITION(!IsInDoc(),
                  "Please remove this from the document properly");
}

NS_IMETHODIMP
nsGenericElement::GetNodeName(nsAString& aNodeName)
{
  mNodeInfo->GetQualifiedName(aNodeName);
  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetLocalName(nsAString& aLocalName)
{
  mNodeInfo->GetLocalName(aLocalName);
  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetNodeValue(nsAString& aNodeValue)
{
  SetDOMStringToNull(aNodeValue);

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::SetNodeValue(const nsAString& aNodeValue)
{
  // The DOM spec says that when nodeValue is defined to be null "setting it
  // has no effect", so we don't throw an exception.
  return NS_OK;
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
  *aParentNode = nsnull;
  nsINode *parent = GetNodeParent();

  return parent ? CallQueryInterface(parent, aParentNode) : NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetPreviousSibling(nsIDOMNode** aPrevSibling)
{
  *aPrevSibling = nsnull;

  nsINode *parent = GetNodeParent();
  if (!parent) {
    return NS_OK;
  }

  PRInt32 pos = parent->IndexOf(this);
  nsIContent *sibling = parent->GetChildAt(pos - 1);

  return sibling ? CallQueryInterface(sibling, aPrevSibling) : NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetNextSibling(nsIDOMNode** aNextSibling)
{
  *aNextSibling = nsnull;

  nsINode *parent = GetNodeParent();
  if (!parent) {
    return NS_OK;
  }

  PRInt32 pos = parent->IndexOf(this);
  nsIContent *sibling = parent->GetChildAt(pos + 1);

  return sibling ? CallQueryInterface(sibling, aNextSibling) : NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  nsIDocument *doc = GetOwnerDoc();
  if (doc) {
    return CallQueryInterface(doc, aOwnerDocument);
  }

  *aOwnerDocument = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetNamespaceURI(nsAString& aNamespaceURI)
{
  return mNodeInfo->GetNamespaceURI(aNamespaceURI);
}

NS_IMETHODIMP
nsGenericElement::GetPrefix(nsAString& aPrefix)
{
  mNodeInfo->GetPrefix(aPrefix);
  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::SetPrefix(const nsAString& aPrefix)
{
  // XXX: Validate the prefix string!

  nsCOMPtr<nsIAtom> prefix;

  if (!aPrefix.IsEmpty()) {
    prefix = do_GetAtom(aPrefix);
    NS_ENSURE_TRUE(prefix, NS_ERROR_OUT_OF_MEMORY);
  }

  if (!nsContentUtils::IsValidNodeName(mNodeInfo->NameAtom(), prefix,
                                       mNodeInfo->NamespaceID())) {
    return NS_ERROR_DOM_NAMESPACE_ERR;
  }

  nsCOMPtr<nsINodeInfo> newNodeInfo;
  nsresult rv = nsContentUtils::PrefixChanged(mNodeInfo, prefix,
                                              getter_AddRefs(newNodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  mNodeInfo = newNodeInfo;

  return NS_OK;
}

nsresult
nsGenericElement::InternalIsSupported(nsISupports* aObject,
                                      const nsAString& aFeature,
                                      const nsAString& aVersion,
                                      PRBool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = PR_FALSE;

  // Convert the incoming UTF16 strings to raw char*'s to save us some
  // code when doing all those string compares.
  NS_ConvertUTF16toUTF8 feature(aFeature);
  NS_ConvertUTF16toUTF8 version(aVersion);

  const char *f = feature.get();
  const char *v = version.get();

  if (PL_strcasecmp(f, "XML") == 0 ||
      PL_strcasecmp(f, "HTML") == 0) {
    if (aVersion.IsEmpty() ||
        PL_strcmp(v, "1.0") == 0 ||
        PL_strcmp(v, "2.0") == 0) {
      *aReturn = PR_TRUE;
    }
  } else if (PL_strcasecmp(f, "Views") == 0 ||
             PL_strcasecmp(f, "StyleSheets") == 0 ||
             PL_strcasecmp(f, "Core") == 0 ||
             PL_strcasecmp(f, "CSS") == 0 ||
             PL_strcasecmp(f, "CSS2") == 0 ||
             PL_strcasecmp(f, "Events") == 0 ||
             PL_strcasecmp(f, "UIEvents") == 0 ||
             PL_strcasecmp(f, "MouseEvents") == 0 ||
             // Non-standard!
             PL_strcasecmp(f, "MouseScrollEvents") == 0 ||
             PL_strcasecmp(f, "HTMLEvents") == 0 ||
             PL_strcasecmp(f, "Range") == 0 ||
             PL_strcasecmp(f, "XHTML") == 0) {
    if (aVersion.IsEmpty() ||
        PL_strcmp(v, "2.0") == 0) {
      *aReturn = PR_TRUE;
    }
  } else if (PL_strcasecmp(f, "XPath") == 0) {
    if (aVersion.IsEmpty() ||
        PL_strcmp(v, "3.0") == 0) {
      *aReturn = PR_TRUE;
    }
  }
#ifdef MOZ_SVG
  else if (PL_strcasecmp(f, "SVGEvents") == 0 ||
           PL_strcasecmp(f, "SVGZoomEvents") == 0 ||
           NS_SVG_TestFeature(aFeature)) {
    if (aVersion.IsEmpty() ||
        PL_strcmp(v, "1.0") == 0 ||
        PL_strcmp(v, "1.1") == 0) {
      *aReturn = PR_TRUE;
    }
  }
#endif /* MOZ_SVG */
  else {
    nsCOMPtr<nsIDOMNSFeatureFactory> factory =
      GetDOMFeatureFactory(aFeature, aVersion);

    if (factory) {
      factory->HasFeature(aObject, aFeature, aVersion, aReturn);
    }
  }
  return NS_OK;
}

nsresult
nsGenericElement::InternalGetFeature(nsISupports* aObject,
                                    const nsAString& aFeature,
                                    const nsAString& aVersion,
                                    nsISupports** aReturn)
{
  *aReturn = nsnull;
  nsCOMPtr<nsIDOMNSFeatureFactory> factory =
    GetDOMFeatureFactory(aFeature, aVersion);

  if (factory) {
    factory->GetFeature(aObject, aFeature, aVersion, aReturn);
  }

  return NS_OK;
}

already_AddRefed<nsIDOMNSFeatureFactory>
nsGenericElement::GetDOMFeatureFactory(const nsAString& aFeature,
                                       const nsAString& aVersion)
{
  nsIDOMNSFeatureFactory *factory = nsnull;
  nsCOMPtr<nsICategoryManager> categoryManager =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID);
  if (categoryManager) {
    nsCAutoString featureCategory(NS_DOMNS_FEATURE_PREFIX);
    AppendUTF16toUTF8(aFeature, featureCategory);
    nsXPIDLCString contractID;
    nsresult rv = categoryManager->GetCategoryEntry(featureCategory.get(),
                                                    NS_ConvertUTF16toUTF8(aVersion).get(),
                                                    getter_Copies(contractID));
    if (NS_SUCCEEDED(rv)) {
      CallGetService(contractID.get(), &factory);  // addrefs
    }
  }
  return factory;
}

NS_IMETHODIMP
nsGenericElement::IsSupported(const nsAString& aFeature,
                              const nsAString& aVersion,
                              PRBool* aReturn)
{
  return InternalIsSupported(this, aFeature, aVersion, aReturn);
}

NS_IMETHODIMP
nsGenericElement::HasAttributes(PRBool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  *aReturn = GetAttrCount() > 0;

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);
  nsDOMSlots *slots = GetDOMSlots();

  if (!slots) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!slots->mAttributeMap) {
    slots->mAttributeMap = new nsDOMAttributeMap(this);
    if (!slots->mAttributeMap) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    if (!slots->mAttributeMap->Init()) {
      slots->mAttributeMap = nsnull;
      return NS_ERROR_FAILURE;
    }
  }

  NS_ADDREF(*aAttributes = slots->mAttributeMap);

  return NS_OK;
}

nsresult
nsGenericElement::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  nsSlots *slots = GetSlots();

  if (!slots) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!slots->mChildNodes) {
    slots->mChildNodes = new nsChildContentList(this);
    if (!slots->mChildNodes) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(slots->mChildNodes);
  }

  NS_ADDREF(*aChildNodes = slots->mChildNodes);

  return NS_OK;
}

nsresult
nsGenericElement::HasChildNodes(PRBool* aReturn)
{
  *aReturn = mAttrsAndChildren.ChildCount() > 0;

  return NS_OK;
}

nsresult
nsGenericElement::GetFirstChild(nsIDOMNode** aNode)
{
  nsIContent *child = mAttrsAndChildren.GetSafeChildAt(0);
  if (child) {
    return CallQueryInterface(child, aNode);
  }

  *aNode = nsnull;

  return NS_OK;
}

nsresult
nsGenericElement::GetLastChild(nsIDOMNode** aNode)
{
  PRUint32 count = mAttrsAndChildren.ChildCount();
  
  if (count > 0) {
    return CallQueryInterface(mAttrsAndChildren.ChildAt(count - 1), aNode);
  }

  *aNode = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetTagName(nsAString& aTagName)
{
  mNodeInfo->GetQualifiedName(aTagName);
  return NS_OK;
}

nsresult
nsGenericElement::GetAttribute(const nsAString& aName,
                               nsAString& aReturn)
{
  const nsAttrName* name = InternalGetExistingAttrNameFromQName(aName);

  if (!name) {
    if (mNodeInfo->NamespaceID() == kNameSpaceID_XUL) {
      // XXX should be SetDOMStringToNull(aReturn);
      // See bug 232598
      aReturn.Truncate();
    }
    else {
      SetDOMStringToNull(aReturn);
    }

    return NS_OK;
  }

  GetAttr(name->NamespaceID(), name->LocalName(), aReturn);

  return NS_OK;
}

nsresult
nsGenericElement::SetAttribute(const nsAString& aName,
                               const nsAString& aValue)
{
  const nsAttrName* name = InternalGetExistingAttrNameFromQName(aName);

  if (!name) {
    nsresult rv = nsContentUtils::CheckQName(aName, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(aName);
    NS_ENSURE_TRUE(nameAtom, NS_ERROR_OUT_OF_MEMORY);

    return SetAttr(kNameSpaceID_None, nameAtom, aValue, PR_TRUE);
  }

  return SetAttr(name->NamespaceID(), name->LocalName(), name->GetPrefix(),
                 aValue, PR_TRUE);
}

nsresult
nsGenericElement::RemoveAttribute(const nsAString& aName)
{
  const nsAttrName* name = InternalGetExistingAttrNameFromQName(aName);

  if (!name) {
    return NS_OK;
  }

  // Hold a strong reference here so that the atom or nodeinfo doesn't go
  // away during UnsetAttr. If it did UnsetAttr would be left with a
  // dangling pointer as argument without knowing it.
  nsAttrName tmp(*name);

  return UnsetAttr(name->NamespaceID(), name->LocalName(), PR_TRUE);
}

nsresult
nsGenericElement::GetAttributeNode(const nsAString& aName,
                                   nsIDOMAttr** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = nsnull;

  nsCOMPtr<nsIDOMNamedNodeMap> map;
  nsresult rv = GetAttributes(getter_AddRefs(map));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> node;
  rv = map->GetNamedItem(aName, getter_AddRefs(node));

  if (NS_SUCCEEDED(rv) && node) {
    rv = CallQueryInterface(node, aReturn);
  }

  return rv;
}

nsresult
nsGenericElement::SetAttributeNode(nsIDOMAttr* aAttribute,
                                   nsIDOMAttr** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  NS_ENSURE_ARG_POINTER(aAttribute);

  *aReturn = nsnull;

  nsCOMPtr<nsIDOMNamedNodeMap> map;
  nsresult rv = GetAttributes(getter_AddRefs(map));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> returnNode;
  rv = map->SetNamedItem(aAttribute, getter_AddRefs(returnNode));
  NS_ENSURE_SUCCESS(rv, rv);

  if (returnNode) {
    rv = CallQueryInterface(returnNode, aReturn);
  }

  return rv;
}

nsresult
nsGenericElement::RemoveAttributeNode(nsIDOMAttr* aAttribute,
                                      nsIDOMAttr** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  NS_ENSURE_ARG_POINTER(aAttribute);

  *aReturn = nsnull;

  nsCOMPtr<nsIDOMNamedNodeMap> map;
  nsresult rv = GetAttributes(getter_AddRefs(map));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString name;

  rv = aAttribute->GetName(name);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDOMNode> node;
    rv = map->RemoveNamedItem(name, getter_AddRefs(node));

    if (NS_SUCCEEDED(rv) && node) {
      rv = CallQueryInterface(node, aReturn);
    }
  }

  return rv;
}

nsresult
nsGenericElement::GetElementsByTagName(const nsAString& aTagname,
                                       nsIDOMNodeList** aReturn)
{
  nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(aTagname);
  NS_ENSURE_TRUE(nameAtom, NS_ERROR_OUT_OF_MEMORY);

  nsContentList *list = NS_GetContentList(this, nameAtom,
                                          kNameSpaceID_Unknown).get();
  NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);

  // transfer ref to aReturn
  *aReturn = list;
  return NS_OK;
}

nsresult
nsGenericElement::GetAttributeNS(const nsAString& aNamespaceURI,
                                 const nsAString& aLocalName,
                                 nsAString& aReturn)
{
  PRInt32 nsid =
    nsContentUtils::NameSpaceManager()->GetNameSpaceID(aNamespaceURI);

  if (nsid == kNameSpaceID_Unknown) {
    // Unknown namespace means no attr...

    aReturn.Truncate();

    return NS_OK;
  }

  nsCOMPtr<nsIAtom> name = do_GetAtom(aLocalName);
  GetAttr(nsid, name, aReturn);

  return NS_OK;
}

nsresult
nsGenericElement::SetAttributeNS(const nsAString& aNamespaceURI,
                                 const nsAString& aQualifiedName,
                                 const nsAString& aValue)
{
  nsCOMPtr<nsINodeInfo> ni;
  nsresult rv =
    nsContentUtils::GetNodeInfoFromQName(aNamespaceURI, aQualifiedName,
                                         mNodeInfo->NodeInfoManager(),
                                         getter_AddRefs(ni));
  NS_ENSURE_SUCCESS(rv, rv);

  return SetAttr(ni->NamespaceID(), ni->NameAtom(), ni->GetPrefixAtom(),
                 aValue, PR_TRUE);
}

nsresult
nsGenericElement::RemoveAttributeNS(const nsAString& aNamespaceURI,
                                    const nsAString& aLocalName)
{
  nsCOMPtr<nsIAtom> name = do_GetAtom(aLocalName);
  PRInt32 nsid =
    nsContentUtils::NameSpaceManager()->GetNameSpaceID(aNamespaceURI);

  if (nsid == kNameSpaceID_Unknown) {
    // Unknown namespace means no attr...

    return NS_OK;
  }

  nsAutoString tmp;
  UnsetAttr(nsid, name, PR_TRUE);

  return NS_OK;
}

nsresult
nsGenericElement::GetAttributeNodeNS(const nsAString& aNamespaceURI,
                                     const nsAString& aLocalName,
                                     nsIDOMAttr** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  *aReturn = nsnull;

  nsCOMPtr<nsIDOMNamedNodeMap> map;
  nsresult rv = GetAttributes(getter_AddRefs(map));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> node;
  rv = map->GetNamedItemNS(aNamespaceURI, aLocalName, getter_AddRefs(node));

  if (NS_SUCCEEDED(rv) && node) {
    rv = CallQueryInterface(node, aReturn);
  }

  return rv;
}

nsresult
nsGenericElement::SetAttributeNodeNS(nsIDOMAttr* aNewAttr,
                                     nsIDOMAttr** aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  NS_ENSURE_ARG_POINTER(aNewAttr);

  *aReturn = nsnull;

  nsCOMPtr<nsIDOMNamedNodeMap> map;
  nsresult rv = GetAttributes(getter_AddRefs(map));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> returnNode;
  rv = map->SetNamedItemNS(aNewAttr, getter_AddRefs(returnNode));
  NS_ENSURE_SUCCESS(rv, rv);

  if (returnNode) {
    rv = CallQueryInterface(returnNode, aReturn);
  }

  return rv;
}

nsresult
nsGenericElement::GetElementsByTagNameNS(const nsAString& aNamespaceURI,
                                         const nsAString& aLocalName,
                                         nsIDOMNodeList** aReturn)
{
  PRInt32 nameSpaceId = kNameSpaceID_Wildcard;

  if (!aNamespaceURI.EqualsLiteral("*")) {
    nsresult rv =
      nsContentUtils::NameSpaceManager()->RegisterNameSpace(aNamespaceURI,
                                                            nameSpaceId);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(aLocalName);
  NS_ENSURE_TRUE(nameAtom, NS_ERROR_OUT_OF_MEMORY);

  nsContentList *list = NS_GetContentList(this, nameAtom, nameSpaceId).get();
  NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);

  // transfer ref to aReturn
  *aReturn = list;
  return NS_OK;
}

nsresult
nsGenericElement::HasAttribute(const nsAString& aName, PRBool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  const nsAttrName* name = InternalGetExistingAttrNameFromQName(aName);
  *aReturn = (name != nsnull);

  return NS_OK;
}

nsresult
nsGenericElement::HasAttributeNS(const nsAString& aNamespaceURI,
                                 const nsAString& aLocalName,
                                 PRBool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  PRInt32 nsid =
    nsContentUtils::NameSpaceManager()->GetNameSpaceID(aNamespaceURI);

  if (nsid == kNameSpaceID_Unknown) {
    // Unknown namespace means no attr...

    *aReturn = PR_FALSE;

    return NS_OK;
  }

  nsCOMPtr<nsIAtom> name = do_GetAtom(aLocalName);
  *aReturn = HasAttr(nsid, name);

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
  // Batch possible DOMSubtreeModified events.
  mozAutoSubtreeModified subtree(GetOwnerDoc(), nsnull);

  nsresult result = NS_OK;
  PRUint32 index, count = GetChildCount();

  for (index = 0; (index < count) && (NS_OK == result); index++) {
    nsIContent *child = GetChildAt(index);

    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(child);
    if (node) {
      PRUint16 nodeType;
      node->GetNodeType(&nodeType);

      switch (nodeType) {
        case nsIDOMNode::TEXT_NODE:

          // ensure that if the text node is empty, it is removed
          if (0 == child->TextLength()) {
            result = RemoveChildAt(index, PR_TRUE);
            if (NS_FAILED(result)) {
              return result;
            }

            count--;
            index--;
            break;
          }
 
          if (index+1 < count) {
            // Get the sibling. If it's also a text node, then
            // remove it from the tree and join the two text
            // nodes.
            nsIContent *sibling = GetChildAt(index + 1);

            nsCOMPtr<nsIDOMNode> siblingNode = do_QueryInterface(sibling);

            if (siblingNode) {
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
nsGenericElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                             nsIContent* aBindingParent,
                             PRBool aCompileEventHandlers)
{
  NS_PRECONDITION(aParent || aDocument, "Must have document if no parent!");
  NS_PRECONDITION(HasSameOwnerDoc(NODE_FROM(aParent, aDocument)),
                  "Must have the same owner document");
  // XXXbz XUL elements are confused about their current doc when they're
  // cloned, so we don't assert if aParent is a XUL element and aDocument is
  // null, even if aParent->GetCurrentDoc() is non-null
  //  NS_PRECONDITION(!aParent || aDocument == aParent->GetCurrentDoc(),
  //                  "aDocument must be current doc of aParent");
  NS_PRECONDITION(!aParent ||
                  (aParent->IsNodeOfType(eXUL) && aDocument == nsnull) ||
                  aDocument == aParent->GetCurrentDoc(),
                  "aDocument must be current doc of aParent");
  NS_PRECONDITION(!GetCurrentDoc(), "Already have a document.  Unbind first!");
  // Note that as we recurse into the kids, they'll have a non-null parent.  So
  // only assert if our parent is _changing_ while we have a parent.
  NS_PRECONDITION(!GetParent() || aParent == GetParent(),
                  "Already have a parent.  Unbind first!");
  NS_PRECONDITION(!GetBindingParent() ||
                  aBindingParent == GetBindingParent() ||
                  (!aBindingParent && aParent &&
                   aParent->GetBindingParent() == GetBindingParent()),
                  "Already have a binding parent.  Unbind first!");
  
  if (!aBindingParent && aParent) {
    aBindingParent = aParent->GetBindingParent();
  }

  // First set the binding parent
  if (aBindingParent) {
    nsDOMSlots *slots = GetDOMSlots();

    if (!slots) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    slots->mBindingParent = aBindingParent; // Weak, so no addref happens.
  }

  // Now set the parent
  if (aParent) {
    mParentPtrBits = NS_REINTERPRET_CAST(PtrBits, aParent) | PARENT_BIT_PARENT_IS_CONTENT;
  }
  else {
    mParentPtrBits = NS_REINTERPRET_CAST(PtrBits, aDocument);
  }

  // XXXbz sXBL/XBL2 issue!

  // Finally, set the document
  if (aDocument) {
    // Notify XBL- & nsIAnonymousContentCreator-generated
    // anonymous content that the document is changing.
    // XXXbz ordering issues here?  Probably not, since ChangeDocumentFor is
    // just pretty broken anyway....  Need to get it working.
    // XXXbz XBL doesn't handle this (asserts), and we don't really want
    // to be doing this during parsing anyway... sort this out.    
    //    aDocument->BindingManager()->ChangeDocumentFor(this, nsnull,
    //                                                   aDocument);

    // Being added to a document.
    mParentPtrBits |= PARENT_BIT_INDOCUMENT;
  }

  // Now recurse into our kids
  nsresult rv;
  PRUint32 i;
  for (i = 0; i < GetChildCount(); ++i) {
    // The child can remove itself from the parent in BindToTree.
    nsCOMPtr<nsIContent> child = mAttrsAndChildren.ChildAt(i);
    rv = child->BindToTree(aDocument, this, aBindingParent,
                           aCompileEventHandlers);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsNodeUtils::ParentChainChanged(this);

  // XXXbz script execution during binding can trigger some of these
  // postcondition asserts....  But we do want that, since things will
  // generally be quite broken when that happens.
  NS_POSTCONDITION(aDocument == GetCurrentDoc(), "Bound to wrong document");
  NS_POSTCONDITION(aParent == GetParent(), "Bound to wrong parent");
  NS_POSTCONDITION(aBindingParent == GetBindingParent(),
                   "Bound to wrong binding parent");

  return NS_OK;
}

void
nsGenericElement::UnbindFromTree(PRBool aDeep, PRBool aNullParent)
{
  NS_PRECONDITION(aDeep || (!GetCurrentDoc() && !GetBindingParent()),
                  "Shallow unbind won't clear document and binding parent on "
                  "kids!");
  // Make sure to unbind this node before doing the kids
  nsIDocument *document = GetCurrentDoc();
  if (document) {
    // Notify XBL- & nsIAnonymousContentCreator-generated
    // anonymous content that the document is changing.
    document->BindingManager()->ChangeDocumentFor(this, document, nsnull);

    if (HasAttr(kNameSpaceID_XLink, nsGkAtoms::href)) {
      document->ForgetLink(this);
    }

    document->ClearBoxObjectFor(this);
  }

  // Unset things in the reverse order from how we set them in BindToTree
  mParentPtrBits = aNullParent ? 0 : mParentPtrBits & ~PARENT_BIT_INDOCUMENT;
  
  nsDOMSlots *slots = GetExistingDOMSlots();
  if (slots) {
    slots->mBindingParent = nsnull;
  }

  if (aDeep) {
    // Do the kids
    PRUint32 i, n = GetChildCount();

    for (i = 0; i < n; ++i) {
      // Note that we pass PR_FALSE for aNullParent here, since we don't want
      // the kids to forget us.  We _do_ want them to forget their binding
      // parent, though, since this only walks non-anonymous kids.
      mAttrsAndChildren.ChildAt(i)->UnbindFromTree(PR_TRUE, PR_FALSE);
    }
  }

  nsNodeUtils::ParentChainChanged(this);
}

nsresult
nsGenericElement::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  return nsGenericElement::doPreHandleEvent(this, aVisitor);
}

nsresult
nsGenericElement::doPreHandleEvent(nsIContent* aContent,
                                   nsEventChainPreVisitor& aVisitor)
{
  //FIXME! Document how this event retargeting works, Bug 329124.
  aVisitor.mCanHandle = PR_TRUE;
  nsCOMPtr<nsIContent> parent = aContent->GetParent();
  if (aContent->IsAnonymousForEvents()) {
    // Don't propagate mutation events which are dispatched somewhere inside
    // native anonymous content.
    if (aVisitor.mEvent->eventStructType == NS_MUTATION_EVENT) {
      aVisitor.mParentTarget = nsnull;
      return NS_OK;
    }

    aVisitor.mEventTargetAtParent = parent;
  } else if (parent) {
    nsCOMPtr<nsIContent> content(do_QueryInterface(aVisitor.mEvent->target));
    if (content && content->GetBindingParent() == parent) {
      aVisitor.mEventTargetAtParent = parent;
    }
  }

  // check for an anonymous parent
  // XXX XBL2/sXBL issue
  nsIDocument* ownerDoc = aContent->GetOwnerDoc();
  if (ownerDoc) {
    nsIContent* insertionParent = ownerDoc->BindingManager()->
      GetInsertionParent(aContent);
    NS_ASSERTION(!(aVisitor.mEventTargetAtParent && insertionParent &&
                   aVisitor.mEventTargetAtParent != insertionParent),
                 "Retargeting and having insertion parent!");
    if (insertionParent) {
      parent = insertionParent;
    }
  }

  if (parent) {
    aVisitor.mParentTarget = parent;
  } else {
    aVisitor.mParentTarget = aContent->GetCurrentDoc();
  }
  return NS_OK;
}

nsresult
nsGenericElement::PostHandleEvent(nsEventChainPostVisitor& /*aVisitor*/)
{
  return NS_OK;
}

nsresult
nsGenericElement::DispatchDOMEvent(nsEvent* aEvent,
                                   nsIDOMEvent* aDOMEvent,
                                   nsPresContext* aPresContext,
                                   nsEventStatus* aEventStatus)
{
  return nsEventDispatcher::DispatchDOMEvent(NS_STATIC_CAST(nsIContent*, this),
                                             aEvent, aDOMEvent,
                                             aPresContext, aEventStatus);
}

nsIAtom*
nsGenericElement::GetID() const
{
  nsIAtom* IDName = GetIDAttributeName();
  if (IDName) {
    const nsAttrValue* attrVal = mAttrsAndChildren.GetAttr(IDName);
    if (attrVal){
      if (attrVal->Type() == nsAttrValue::eAtom) {
        return attrVal->GetAtomValue();
      }
      if(attrVal->IsEmptyString()){
        return nsnull;
      }
      // Check if the ID has been stored as a string.
      // This would occur if the ID attribute name changed after 
      // the ID was parsed. 
      if (attrVal->Type() == nsAttrValue::eString) {
        nsAutoString idVal(attrVal->GetStringValue());

        // Create an atom from the value and set it into the attribute list. 
        NS_CONST_CAST(nsAttrValue*, attrVal)->ParseAtom(idVal);
        return attrVal->GetAtomValue();
      }
    }
  }
  return nsnull;
}

const nsAttrValue*
nsGenericElement::GetClasses() const
{
  return nsnull;
}

NS_IMETHODIMP
nsGenericElement::WalkContentStyleRules(nsRuleWalker* aRuleWalker)
{
  return NS_OK;
}

nsICSSStyleRule*
nsGenericElement::GetInlineStyleRule()
{
  return nsnull;
}

NS_IMETHODIMP
nsGenericElement::SetInlineStyleRule(nsICSSStyleRule* aStyleRule,
                                     PRBool aNotify)
{
  NS_NOTYETIMPLEMENTED("nsGenericElement::SetInlineStyleRule");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP_(PRBool)
nsGenericElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  return PR_FALSE;
}

nsChangeHint
nsGenericElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                         PRInt32 aModType) const
{
  return nsChangeHint(0);
}

nsIAtom *
nsGenericElement::GetIDAttributeName() const
{
  return mNodeInfo->GetIDAttributeAtom();
}

nsIAtom *
nsGenericElement::GetClassAttributeName() const
{
  return nsnull;
}

PRBool
nsGenericElement::FindAttributeDependence(const nsIAtom* aAttribute,
                                          const MappedAttributeEntry* const aMaps[],
                                          PRUint32 aMapCount)
{
  for (PRUint32 mapindex = 0; mapindex < aMapCount; ++mapindex) {
    for (const MappedAttributeEntry* map = aMaps[mapindex];
         map->attribute; ++map) {
      if (aAttribute == *map->attribute) {
        return PR_TRUE;
      }
    }
  }

  return PR_FALSE;
}

already_AddRefed<nsINodeInfo>
nsGenericElement::GetExistingAttrNameFromQName(const nsAString& aStr) const
{
  const nsAttrName* name = InternalGetExistingAttrNameFromQName(aStr);
  if (!name) {
    return nsnull;
  }

  nsINodeInfo* nodeInfo;
  if (name->IsAtom()) {
    mNodeInfo->NodeInfoManager()->GetNodeInfo(name->Atom(), nsnull,
                                              kNameSpaceID_None, &nodeInfo);
  }
  else {
    NS_ADDREF(nodeInfo = name->NodeInfo());
  }

  return nodeInfo;
}

already_AddRefed<nsIURI>
nsGenericElement::GetBaseURI() const
{
  nsIDocument* doc = GetOwnerDoc();
  if (!doc) {
    // We won't be able to do security checks, etc.  So don't go any
    // further.  That said, this really shouldn't happen...
    NS_ERROR("Element without owner document");
    return nsnull;
  }

  // Our base URL depends on whether we have an xml:base attribute, as
  // well as on whether any of our ancestors do.
  nsCOMPtr<nsIURI> parentBase;

  nsIContent *parent = GetParent();
  if (parent) {
    parentBase = parent->GetBaseURI();
  } else {
    // No parent, so just use the document (we must be the root or not in the
    // tree).
    parentBase = doc->GetBaseURI();
  }
  
  // Now check for an xml:base attr 
  nsAutoString value;
  GetAttr(kNameSpaceID_XML, nsGkAtoms::base, value);
  if (value.IsEmpty()) {
    // No xml:base, so we just use the parent's base URL
    nsIURI *base = nsnull;
    parentBase.swap(base);

    return base;
  }

  nsCOMPtr<nsIURI> ourBase;
  nsresult rv = NS_NewURI(getter_AddRefs(ourBase), value,
                          doc->GetDocumentCharacterSet().get(), parentBase);
  if (NS_SUCCEEDED(rv)) {
    // do a security check, almost the same as nsDocument::SetBaseURL()
    rv = nsContentUtils::GetSecurityManager()->
      CheckLoadURIWithPrincipal(NodePrincipal(), ourBase,
                                nsIScriptSecurityManager::STANDARD);
  }

  nsIURI *base;
  if (NS_FAILED(rv)) {
    base = parentBase;
  } else {
    base = ourBase;
  }

  NS_IF_ADDREF(base);

  return base;    
}

PRBool
nsGenericElement::IsLink(nsIURI** aURI) const
{
  *aURI = nsnull;
  return PR_FALSE;
}

void
nsGenericElement::SetFocus(nsPresContext* aPresContext)
{
  // Traditionally focusable elements can take focus as long as they don't set
  // the disabled attribute

  nsCOMPtr<nsIPresShell> presShell = aPresContext->PresShell();
  if (!presShell) {
    return;
  }
  nsIFrame* frame = presShell->GetPrimaryFrameFor(this);
  if (frame && frame->IsFocusable() &&
      aPresContext->EventStateManager()->SetContentState(this,
                                                         NS_EVENT_STATE_FOCUS)) {
    presShell->ScrollContentIntoView(this, NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,
                                     NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE);
  }
}

// static
PRBool
nsGenericElement::ShouldFocus(nsIContent *aContent)
{
  // Default to false, since if the document is not attached to a window,
  // we should not focus any of its content.
  PRBool visible = PR_FALSE;

  // Figure out if we're focusing an element in an inactive (hidden)
  // tab (whose docshell is not visible), if so, drop this focus
  // request on the floor

  nsIDocument *document = aContent->GetDocument();

  if (document) {
    nsIScriptGlobalObject *sgo = document->GetScriptGlobalObject();

    if (sgo) {
      nsCOMPtr<nsIWebNavigation> webNav(do_GetInterface(sgo));
      nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(webNav));

      if (baseWin) {
        baseWin->GetVisibility(&visible);
      }
    }
  }

  return visible;
}

// static
PRBool
nsGenericElement::ShouldBlur(nsIContent *aContent)
{
  // Determine if the current element is focused, if it is not focused
  // then we should not try to blur
  PRBool isFocused = PR_FALSE;

  nsIDocument *document = aContent->GetDocument();

  if (document) {
    nsPIDOMWindow *win = document->GetWindow();

    if (win) {
      nsCOMPtr<nsIFocusController> focusController =
           win->GetRootFocusController();

      if (focusController) {
        nsCOMPtr<nsIDOMElement> focusedElement;
        focusController->GetFocusedElement(getter_AddRefs(focusedElement));    
        nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(aContent);
        //when the element is the same as the focused element, blur it
        if (domElement == focusedElement)
          isFocused = PR_TRUE;
      }
    }
  }

  return isFocused;
}

nsIContent*
nsGenericElement::GetBindingParent() const
{
  nsDOMSlots *slots = GetExistingDOMSlots();

  if (slots) {
    return slots->mBindingParent;
  }
  return nsnull;
}

PRBool
nsGenericElement::IsNodeOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~(eCONTENT | eELEMENT));
}

//----------------------------------------------------------------------

// virtual
void
nsGenericElement::SetMayHaveFrame(PRBool aMayHaveFrame)
{
  if (aMayHaveFrame) {
    SetFlags(NODE_MAY_HAVE_FRAME);
  } else {
    UnsetFlags(NODE_MAY_HAVE_FRAME);
  }
}

// virtual
PRBool
nsGenericElement::MayHaveFrame() const
{
  return HasFlag(NODE_MAY_HAVE_FRAME);
}

PRUint32
nsGenericElement::GetScriptTypeID() const
{
    PtrBits flags = GetFlags();

    /* 4 bits reserved for script-type ID. */
    return (flags >> NODE_SCRIPT_TYPE_OFFSET) & 0x000F;
}

nsresult
nsGenericElement::SetScriptTypeID(PRUint32 aLang)
{
    if ((aLang & 0x000F) != aLang) {
        NS_ERROR("script ID too large!");
        return NS_ERROR_FAILURE;
    }
    /* SetFlags will just mask in the specific flags set, leaving existing
       ones alone.  So we must clear all the bits first */
    UnsetFlags(0x000FU << NODE_SCRIPT_TYPE_OFFSET);
    SetFlags(aLang << NODE_SCRIPT_TYPE_OFFSET);
    return NS_OK;
}

nsresult
nsGenericElement::InsertChildAt(nsIContent* aKid,
                                PRUint32 aIndex,
                                PRBool aNotify)
{
  NS_PRECONDITION(aKid, "null ptr");

  return doInsertChildAt(aKid, aIndex, aNotify, this, GetCurrentDoc(),
                         mAttrsAndChildren);
}


/* static */
nsresult
nsGenericElement::doInsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                  PRBool aNotify, nsIContent* aParent,
                                  nsIDocument* aDocument,
                                  nsAttrAndChildArray& aChildArray)
{
  NS_PRECONDITION(aParent || aDocument, "Must have document if no parent!");
  NS_PRECONDITION(!aParent || aParent->GetCurrentDoc() == aDocument,
                  "Incorrect aDocument");

  nsresult rv;
  nsINode* container = NODE_FROM(aParent, aDocument);
  if (!container->HasSameOwnerDoc(aKid)) {
    if (aKid->GetOwnerDoc()) {
      return NS_ERROR_DOM_WRONG_DOCUMENT_ERR;
    }

    nsCOMPtr<nsIDOMNode> kid = do_QueryInterface(aKid, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint16 nodeType = 0;
    rv = kid->GetNodeType(&nodeType);
    NS_ENSURE_SUCCESS(rv, rv);

    // DocumentType nodes are the only nodes that can have a null ownerDocument
    // according to the DOM spec, and we need to allow inserting them.
    if (nodeType != nsIDOMNode::DOCUMENT_TYPE_NODE) {
      return NS_ERROR_DOM_WRONG_DOCUMENT_ERR;
    }
  }

  PRUint32 childCount = aChildArray.ChildCount();
  NS_ENSURE_TRUE(aIndex <= childCount, NS_ERROR_ILLEGAL_VALUE);

  nsMutationGuard::DidMutate();

  PRBool isAppend = (aIndex == childCount);

  mozAutoDocUpdate updateBatch(aDocument, UPDATE_CONTENT_MODEL, aNotify);

  rv = aChildArray.InsertChildAt(aKid, aIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aKid->BindToTree(aDocument, aParent, nsnull, PR_TRUE);
  if (NS_FAILED(rv)) {
    aChildArray.RemoveChildAt(aIndex);
    aKid->UnbindFromTree();
    return rv;
  }

  // The kid may have removed its parent from the document, so recheck that
  // that's still in the document before proceeding.  Also, the kid may have
  // just removed itself, in which case we don't really want to fire
  // ContentAppended or a mutation event.
  // XXXbz What if the kid just moved us in the document?  Scripts suck.  We
  // really need to stop running them while we're in the middle of modifying
  // the DOM....

  if (aNotify && aKid->GetNodeParent() == container) {
    // Note that we always want to call ContentInserted when things are added
    // as kids to documents
    if (aParent && isAppend) {
      nsNodeUtils::ContentAppended(aParent, aIndex);
    } else {
      nsNodeUtils::ContentInserted(container, aKid, aIndex);
    }

    if (nsContentUtils::HasMutationListeners(aKid,
          NS_EVENT_BITS_MUTATION_NODEINSERTED)) {
      nsMutationEvent mutation(PR_TRUE, NS_MUTATION_NODEINSERTED);
      mutation.mRelatedNode = do_QueryInterface(container);
      mozAutoSubtreeModified subtree(container->GetOwnerDoc(), container);
      nsEventDispatcher::Dispatch(aKid, nsnull, &mutation);
    }
  }

  return NS_OK;
}

nsresult
nsGenericElement::RemoveChildAt(PRUint32 aIndex, PRBool aNotify)
{
  nsCOMPtr<nsIContent> oldKid = mAttrsAndChildren.GetSafeChildAt(aIndex);
  NS_ASSERTION(oldKid == GetChildAt(aIndex), "Unexpected child in RemoveChildAt");

  if (oldKid) {
    return doRemoveChildAt(aIndex, aNotify, oldKid, this, GetCurrentDoc(),
                           mAttrsAndChildren);
  }

  return NS_OK;
}

/* static */
nsresult
nsGenericElement::doRemoveChildAt(PRUint32 aIndex, PRBool aNotify,
                                  nsIContent* aKid, nsIContent* aParent,
                                  nsIDocument* aDocument,
                                  nsAttrAndChildArray& aChildArray)
{
  NS_PRECONDITION(aParent || aDocument, "Must have document if no parent!");
  NS_PRECONDITION(!aParent || aParent->GetCurrentDoc() == aDocument,
                  "Incorrect aDocument");

  nsMutationGuard::DidMutate();

  nsINode* container = NODE_FROM(aParent, aDocument);
  
  NS_PRECONDITION(aKid && aKid->GetParent() == aParent &&
                  aKid == container->GetChildAt(aIndex) &&
                  container->IndexOf(aKid) == (PRInt32)aIndex, "Bogus aKid");

  mozAutoDocUpdate updateBatch(aDocument, UPDATE_CONTENT_MODEL, aNotify);

  nsMutationGuard guard;

  mozAutoSubtreeModified subtree(nsnull, nsnull);
  if (aNotify &&
      nsContentUtils::HasMutationListeners(aKid,
        NS_EVENT_BITS_MUTATION_NODEREMOVED)) {
    nsMutationEvent mutation(PR_TRUE, NS_MUTATION_NODEREMOVED);
    mutation.mRelatedNode = do_QueryInterface(container);
    subtree.UpdateTarget(container->GetOwnerDoc(), container);
    nsEventDispatcher::Dispatch(aKid, nsnull, &mutation);
  }

  // Someone may have removed the kid or any of its siblings while that event
  // was processing.
  if (guard.Mutated(0)) {
    aIndex = container->IndexOf(aKid);
    if (NS_STATIC_CAST(PRInt32, aIndex) < 0) {
      return NS_OK;
    }
  }

  aChildArray.RemoveChildAt(aIndex);

  if (aNotify) {
    nsNodeUtils::ContentRemoved(container, aKid, aIndex);
  }

  aKid->UnbindFromTree();

  return NS_OK;
}

/* static */
nsresult
nsGenericElement::DispatchEvent(nsPresContext* aPresContext,
                                nsEvent* aEvent,
                                nsIContent* aTarget,
                                PRBool aFullDispatch,
                                nsEventStatus* aStatus)
{
  NS_PRECONDITION(aTarget, "Must have target");
  NS_PRECONDITION(aEvent, "Must have source event");
  NS_PRECONDITION(aStatus, "Null out param?");

  if (!aPresContext) {
    return NS_OK;
  }

  nsIPresShell *shell = aPresContext->GetPresShell();
  if (!shell) {
    return NS_OK;
  }

  if (aFullDispatch) {
    return shell->HandleEventWithTarget(aEvent, nsnull, aTarget, aStatus);
  }

  return shell->HandleDOMEventWithTarget(aTarget, aEvent, aStatus);
}

/* static */
nsresult
nsGenericElement::DispatchClickEvent(nsPresContext* aPresContext,
                                     nsInputEvent* aSourceEvent,
                                     nsIContent* aTarget,
                                     PRBool aFullDispatch,
                                     nsEventStatus* aStatus)
{
  NS_PRECONDITION(aTarget, "Must have target");
  NS_PRECONDITION(aSourceEvent, "Must have source event");
  NS_PRECONDITION(aStatus, "Null out param?");

  nsMouseEvent event(NS_IS_TRUSTED_EVENT(aSourceEvent), NS_MOUSE_CLICK,
                     aSourceEvent->widget, nsMouseEvent::eReal);
  event.refPoint = aSourceEvent->refPoint;
  PRUint32 clickCount = 1;
  if (aSourceEvent->eventStructType == NS_MOUSE_EVENT) {
    clickCount = NS_STATIC_CAST(nsMouseEvent*, aSourceEvent)->clickCount;
  }
  event.clickCount = clickCount;
  event.isShift = aSourceEvent->isShift;
  event.isControl = aSourceEvent->isControl;
  event.isAlt = aSourceEvent->isAlt;
  event.isMeta = aSourceEvent->isMeta;

  return DispatchEvent(aPresContext, &event, aTarget, aFullDispatch, aStatus);
}

nsIFrame*
nsGenericElement::GetPrimaryFrame()
{
  nsIDocument* doc = GetCurrentDoc();
  if (!doc) {
    return nsnull;
  }

  return GetPrimaryFrameFor(this, doc);
}

nsIFrame*
nsGenericElement::GetPrimaryFrame(mozFlushType aType)
{
  nsIDocument* doc = GetCurrentDoc();
  if (!doc) {
    return nsnull;
  }

  // Cause a flush, so we get up-to-date frame
  // information
  doc->FlushPendingNotifications(aType);

  return GetPrimaryFrameFor(this, doc);
}

/* static */
nsIFrame*
nsGenericElement::GetPrimaryFrameFor(nsIContent* aContent,
                                     nsIDocument* aDocument)
{
  // Get presentation shell 0
  nsIPresShell *presShell = aDocument->GetShellAt(0);
  if (!presShell) {
    return nsnull;
  }

  return presShell->GetPrimaryFrameFor(aContent);
}

//----------------------------------------------------------------------

// Generic DOMNode implementations

/*
 * This helper function checks if aChild is the same as aNode or if
 * aChild is one of aNode's ancestors. -- jst@citec.fi
 */

NS_IMETHODIMP
nsGenericElement::InsertBefore(nsIDOMNode *aNewChild, nsIDOMNode *aRefChild,
                               nsIDOMNode **aReturn)
{
  return doReplaceOrInsertBefore(PR_FALSE, aNewChild, aRefChild, this, GetCurrentDoc(),
                                 aReturn);
}

NS_IMETHODIMP
nsGenericElement::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                               nsIDOMNode** aReturn)
{
  return doReplaceOrInsertBefore(PR_TRUE, aNewChild, aOldChild, this, GetCurrentDoc(),
                                 aReturn);
}

NS_IMETHODIMP
nsGenericElement::RemoveChild(nsIDOMNode *aOldChild, nsIDOMNode **aReturn)
{
  return doRemoveChild(aOldChild, this, GetCurrentDoc(),
                       aReturn);
}

// When replacing, aRefContent is the content being replaced; when
// inserting it's the content before which we're inserting.  In the
// latter case it may be null.
static
PRBool IsAllowedAsChild(nsIContent* aNewChild, PRUint16 aNewNodeType,
                        nsIContent* aParent, nsIDocument* aDocument,
                        PRBool aIsReplace, nsIContent* aRefContent)
{
  NS_PRECONDITION(aNewChild, "Must have new child");
  NS_PRECONDITION(!aIsReplace || aRefContent,
                  "Must have ref content for replace");
#ifdef DEBUG
  PRUint16 debugNodeType = 0;
  nsCOMPtr<nsIDOMNode> debugNode(do_QueryInterface(aNewChild));
  nsresult debugRv = debugNode->GetNodeType(&debugNodeType);

  NS_PRECONDITION(NS_SUCCEEDED(debugRv) && debugNodeType == aNewNodeType,
                  "Bogus node type passed");
#endif

  if (aParent && nsContentUtils::ContentIsDescendantOf(aParent, aNewChild)) {
    return PR_FALSE;
  }

  // The allowed child nodes differ for documents and elements
  switch (aNewNodeType) {
  case nsIDOMNode::COMMENT_NODE :
  case nsIDOMNode::PROCESSING_INSTRUCTION_NODE :
    // OK in both cases
    return PR_TRUE;
  case nsIDOMNode::TEXT_NODE :
  case nsIDOMNode::CDATA_SECTION_NODE :
  case nsIDOMNode::ENTITY_REFERENCE_NODE :
    // Only allowed under elements
    return aParent != nsnull;
  case nsIDOMNode::ELEMENT_NODE :
    {
      if (aParent) {
        // Always ok to have elements under other elements
        return PR_TRUE;
      }

      nsIContent* rootContent = aDocument->GetRootContent();
      if (rootContent) {
        // Already have a documentElement, so this is only OK if we're
        // replacing it.
        return aIsReplace && rootContent == aRefContent;
      }

      // We don't have a documentElement yet.  Our one remaining constraint is
      // that the documentElement must come after the doctype.
      if (!aRefContent) {
        // Appending is just fine.
        return PR_TRUE;
      }

      // Now grovel for a doctype
      nsCOMPtr<nsIDOMDocument> doc = do_QueryInterface(aDocument);
      NS_ASSERTION(doc, "Shouldn't happen");
      nsCOMPtr<nsIDOMDocumentType> docType;
      doc->GetDoctype(getter_AddRefs(docType));
      nsCOMPtr<nsIContent> docTypeContent = do_QueryInterface(docType);
      
      if (!docTypeContent) {
        // It's all good.
        return PR_TRUE;
      }

      PRInt32 doctypeIndex = aDocument->IndexOf(docTypeContent);
      PRInt32 insertIndex = aDocument->IndexOf(aRefContent);

      // Now we're OK in the following two cases only:
      // 1) We're replacing something that's not before the doctype
      // 2) We're inserting before something that comes after the doctype 
      return aIsReplace ? (insertIndex >= doctypeIndex) :
        insertIndex > doctypeIndex;
    }
  case nsIDOMNode::DOCUMENT_TYPE_NODE :
    {
      if (aParent) {
        // no doctypes allowed under elements
        return PR_FALSE;
      }

      nsCOMPtr<nsIDOMDocument> doc = do_QueryInterface(aDocument);
      NS_ASSERTION(doc, "Shouldn't happen");
      nsCOMPtr<nsIDOMDocumentType> docType;
      doc->GetDoctype(getter_AddRefs(docType));
      nsCOMPtr<nsIContent> docTypeContent = do_QueryInterface(docType);
      if (docTypeContent) {
        // Already have a doctype, so this is only OK if we're replacing it
        return aIsReplace && docTypeContent == aRefContent;
      }

      // We don't have a doctype yet.  Our one remaining constraint is
      // that the doctype must come before the documentElement.
      nsIContent* rootContent = aDocument->GetRootContent();
      if (!rootContent) {
        // It's all good
        return PR_TRUE;
      }

      if (!aRefContent) {
        // Trying to append a doctype, but have a documentElement
        return PR_FALSE;
      }

      PRInt32 rootIndex = aDocument->IndexOf(rootContent);
      PRInt32 insertIndex = aDocument->IndexOf(aRefContent);

      // Now we're OK if and only if insertIndex <= rootIndex.  Indeed, either
      // we end up replacing aRefContent or we end up before it.  Either one is
      // ok as long as aRefContent is not after rootContent.
      return insertIndex <= rootIndex;
    }
  case nsIDOMNode::DOCUMENT_FRAGMENT_NODE :
    {
      // Note that for now we only allow nodes inside document fragments if
      // they're allowed inside elements.  If we ever change this to allow
      // doctype nodes in document fragments, we'll need to update this code
      if (aParent) {
        // All good here
        return PR_TRUE;
      }

      PRBool sawElement = PR_FALSE;
      PRUint32 count = aNewChild->GetChildCount();
      for (PRUint32 index = 0; index < count; ++index) {
        nsIContent* childContent = aNewChild->GetChildAt(index);
        NS_ASSERTION(childContent, "Something went wrong");
        if (childContent->IsNodeOfType(nsINode::eELEMENT)) {
          if (sawElement) {
            // Can't put two elements into a document
            return PR_FALSE;
          }
          sawElement = PR_TRUE;
        }
        // If we can put this content at the the right place, we might be ok;
        // if not, we bail out.
        nsCOMPtr<nsIDOMNode> childNode(do_QueryInterface(childContent));
        PRUint16 type;
        childNode->GetNodeType(&type);
        if (!IsAllowedAsChild(childContent, type, aParent, aDocument,
                              aIsReplace, aRefContent)) {
          return PR_FALSE;
        }
      }

      // Everything in the fragment checked out ok, so we can stick it in here
      return PR_TRUE;
    }
  default:
    /*
     * aNewChild is of invalid type.
     */
    break;
  }

  return PR_FALSE;
}

/* static */
nsresult
nsGenericElement::doReplaceOrInsertBefore(PRBool aReplace,
                                          nsIDOMNode* aNewChild,
                                          nsIDOMNode* aRefChild,
                                          nsIContent* aParent,
                                          nsIDocument* aDocument,
                                          nsIDOMNode** aReturn)
{
  NS_PRECONDITION(aParent || aDocument, "Must have document if no parent!");
  NS_PRECONDITION(!aParent || aParent->GetCurrentDoc() == aDocument,
                  "Incorrect aDocument");

  *aReturn = nsnull;

  if (!aNewChild || (aReplace && !aRefChild)) {
    return NS_ERROR_NULL_POINTER;
  }

  // Keep a strong reference to the node that we'll return to ensure it
  // doesn't go away.
  nsCOMPtr<nsIDOMNode> returnVal = aReplace ? aRefChild : aNewChild;

  nsCOMPtr<nsIContent> refContent;
  nsresult res = NS_OK;
  PRInt32 insPos;

  nsINode* container = NODE_FROM(aParent, aDocument);

  // Figure out which index to insert at
  if (aRefChild) {
    refContent = do_QueryInterface(aRefChild);
    insPos = container->IndexOf(refContent);
    if (insPos < 0) {
      return NS_ERROR_DOM_NOT_FOUND_ERR;
    }

    if (aRefChild == aNewChild) {
      NS_ADDREF(*aReturn = aNewChild);

      return NS_OK;
    }
  } else {
    insPos = container->GetChildCount();
  }

  nsCOMPtr<nsIContent> newContent = do_QueryInterface(aNewChild);
  if (!newContent) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  PRUint16 nodeType = 0;
  res = aNewChild->GetNodeType(&nodeType);
  NS_ENSURE_SUCCESS(res, res);

  // Make sure that the inserted node is allowed as a child of its new parent.
  if (!IsAllowedAsChild(newContent, nodeType, aParent, aDocument, aReplace,
                        refContent)) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  if (!container->HasSameOwnerDoc(newContent)) {
    // DocumentType nodes are the only nodes that can have a null ownerDocument
    // according to the DOM spec, and we need to allow inserting them.
    if (nodeType != nsIDOMNode::DOCUMENT_TYPE_NODE ||
        newContent->GetOwnerDoc()) {
      return NS_ERROR_DOM_WRONG_DOCUMENT_ERR;
    }

    if (!nsContentUtils::CanCallerAccess(aNewChild)) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }
  }

  // We want an update batch when we expect several mutations to be performed,
  // which is when we're replacing a node, or when we're inserting a fragment.
  mozAutoDocUpdate updateBatch(aDocument, UPDATE_CONTENT_MODEL,
    aReplace || nodeType == nsIDOMNode::DOCUMENT_FRAGMENT_NODE);

  // If we're replacing
  if (aReplace) {
    // Getting (and addrefing) the following child here is sort of wasteful
    // in the common case, but really, it's not that expensive. Get over it.
    refContent = container->GetChildAt(insPos + 1);

    nsMutationGuard guard;

    res = container->RemoveChildAt(insPos, PR_TRUE);
    NS_ENSURE_SUCCESS(res, res);

    if (guard.Mutated(1)) {
      insPos = refContent ? container->IndexOf(refContent) :
                            container->GetChildCount();
      if (insPos < 0) {
        return NS_ERROR_DOM_NOT_FOUND_ERR;
      }

      // Passing PR_FALSE for aIsReplace since we now have removed the node
      // to be replaced.
      if (!IsAllowedAsChild(newContent, nodeType, aParent, aDocument,
                            PR_FALSE, refContent)) {
        return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
      }
    }
  }

  /*
   * Check if we're inserting a document fragment. If we are, we need
   * to remove the children of the document fragment and add them
   * individually (i.e. we don't add the actual document fragment).
   */
  if (nodeType == nsIDOMNode::DOCUMENT_FRAGMENT_NODE) {
    PRUint32 count = newContent->GetChildCount();

    // Copy the children into a separate array to avoid having to deal with
    // mutations to the fragment while we're inserting.
    nsCOMArray<nsIContent> fragChildren;
    if (!fragChildren.SetCapacity(count)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    PRUint32 i;
    for (i = 0; i < count; i++) {
      nsIContent* child = newContent->GetChildAt(i);
      NS_ASSERTION(child->GetCurrentDoc() == nsnull,
                   "How did we get a child with a current doc?");
      fragChildren.AppendObject(child);
    }

    // Remove the children from the fragment and flag for possible mutations.
    PRBool mutated = PR_FALSE;
    for (i = count; i > 0;) {
      // We don't need to update i if someone mutates the DOM. The only thing
      // that'd happen is that the resulting child list might be unexpected,
      // but we should never crash since RemoveChildAt is out-of-bounds safe.
      nsMutationGuard guard;
      newContent->RemoveChildAt(--i, PR_TRUE);
      mutated = mutated || guard.Mutated(1);
    }

    // Iterate through the fragment's children, and insert them in the new
    // parent
    for (i = 0; i < count; ++i) {
      // Get the n:th child from the array.
      nsIContent* childContent = fragChildren[i];

      // If we've had any unexpeted mutations so far we need to recheck that
      // the child can still be inserted.
      if (mutated) {
        // We really only need to update insPos if we *just* got an unexpected
        // mutation as opposed to 3 insertions ago. But this is an edgecase so
        // no need to over optimize.
        insPos = refContent ? container->IndexOf(refContent) :
                              container->GetChildCount();
        if (insPos < 0) {
          // Someone seriously messed up the childlist. We have no idea
          // where to insert the remaining children, so just bail.
          return NS_ERROR_DOM_NOT_FOUND_ERR;
        }

        nsCOMPtr<nsIDOMNode> tmpNode = do_QueryInterface(childContent);
        PRUint16 tmpType = 0;
        tmpNode->GetNodeType(&tmpType);

        if (childContent->GetNodeParent() ||
            !IsAllowedAsChild(childContent, tmpType, aParent, aDocument, PR_FALSE,
                              refContent)) {
          return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
        }
      }

      nsMutationGuard guard;

      // XXXbz how come no reparenting here?  That seems odd...
      // Insert the child.
      res = container->InsertChildAt(childContent, insPos, PR_TRUE);
      NS_ENSURE_SUCCESS(res, res);

      // Check to see if any evil mutation events mucked around with the
      // child list.
      mutated = mutated || guard.Mutated(1);
      
      ++insPos;
    }
  }
  else {
    // Not inserting a fragment but rather a single node.
    PRBool newContentIsXUL = newContent->IsNodeOfType(eXUL);

    // Remove the element from the old parent if one exists
    nsINode* oldParent = newContent->GetNodeParent();

    if (oldParent) {
      PRInt32 removeIndex = oldParent->IndexOf(newContent);

      if (removeIndex < 0) {
        // newContent is anonymous.  We can't deal with this, so just bail
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      }
      
      NS_ASSERTION(!(oldParent == container && removeIndex == insPos),
                   "invalid removeIndex");

      nsMutationGuard guard;

      res = oldParent->RemoveChildAt(removeIndex, PR_TRUE);
      NS_ENSURE_SUCCESS(res, res);

      // Adjust insert index if the node we ripped out was a sibling
      // of the node we're inserting before
      if (oldParent == container && removeIndex < insPos) {
        --insPos;
      }

      if (guard.Mutated(1)) {
        insPos = refContent ? container->IndexOf(refContent) :
                              container->GetChildCount();
        if (insPos < 0) {
          // Someone seriously messed up the childlist. We have no idea
          // where to insert the new child, so just bail.
          return NS_ERROR_DOM_NOT_FOUND_ERR;
        }

        if (newContent->GetNodeParent() ||
            !IsAllowedAsChild(newContent, nodeType, aParent, aDocument,
                              PR_FALSE, refContent)) {
          return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
        }
      }
    }

    if (!newContentIsXUL) {
      nsContentUtils::ReparentContentWrapper(newContent, aParent,
                                             container->GetOwnerDoc(),
                                             container->GetOwnerDoc());
    }

    res = container->InsertChildAt(newContent, insPos, PR_TRUE);
    NS_ENSURE_SUCCESS(res, res);
  }

  returnVal.swap(*aReturn);

  return res;
}

/* static */
nsresult
nsGenericElement::doRemoveChild(nsIDOMNode* aOldChild, nsIContent* aParent,
                                nsIDocument* aDocument, nsIDOMNode** aReturn)
{
  NS_PRECONDITION(aParent || aDocument, "Must have document if no parent!");
  NS_PRECONDITION(!aParent || aParent->GetCurrentDoc() == aDocument,
                  "Incorrect aDocument");

  *aReturn = nsnull;
  NS_ENSURE_TRUE(aOldChild, NS_ERROR_NULL_POINTER);

  nsINode* container = NODE_FROM(aParent, aDocument);

  nsCOMPtr<nsIContent> content = do_QueryInterface(aOldChild);
  // fix children to be a passed argument
  PRInt32 index = container->IndexOf(content);
  if (index == -1) {
    // aOldChild isn't one of our children.
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  nsresult rv = container->RemoveChildAt(index, PR_TRUE);

  *aReturn = aOldChild;
  NS_ADDREF(aOldChild);

  return rv;
}

//----------------------------------------------------------------------

// nsISupports implementation

NS_IMPL_CYCLE_COLLECTION_CLASS(nsGenericElement)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsGenericElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_LISTENERMANAGER
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER

  // Unlink child content (and unbind our subtree).
  {
    PRUint32 i;
    PRUint32 kids = tmp->mAttrsAndChildren.ChildCount();
    for (i = kids; i > 0; i--) {
      tmp->mAttrsAndChildren.ChildAt(i-1)->UnbindFromTree();
      tmp->mAttrsAndChildren.RemoveChildAt(i-1);    
    }
  }  

  // Unlink any DOM slots of interest.
  {
    nsDOMSlots *slots = tmp->GetExistingDOMSlots();
    if (slots)
      slots->mAttributeMap = nsnull;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsGenericElement)
  nsIDocument* ownerDoc = tmp->GetOwnerDoc();
  if (ownerDoc) {
    ownerDoc->BindingManager()->Traverse(tmp, cb);
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_LISTENERMANAGER
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_PRESERVED_WRAPPER

  if (tmp->HasProperties() && tmp->IsNodeOfType(nsINode::eXUL)) {
    nsISupports* property =
      NS_STATIC_CAST(nsISupports*,
                     tmp->GetProperty(nsGkAtoms::contextmenulistener));
    cb.NoteXPCOMChild(property);
    property = NS_STATIC_CAST(nsISupports*,
                              tmp->GetProperty(nsGkAtoms::popuplistener));
    cb.NoteXPCOMChild(property);
  }

  // Traverse child content.
  {
    PRUint32 i;
    PRUint32 kids = tmp->mAttrsAndChildren.ChildCount();
    for (i = 0; i < kids; i++)
      cb.NoteXPCOMChild(tmp->mAttrsAndChildren.GetSafeChildAt(i));
  }

  // Traverse any DOM slots of interest.
  {
    nsDOMSlots *slots = tmp->GetExistingDOMSlots();
    if (slots) {
      cb.NoteXPCOMChild(slots->mAttributeMap.get());
      if (tmp->IsNodeOfType(nsINode::eXUL))
        cb.NoteXPCOMChild(slots->mControllers);
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END


NS_INTERFACE_MAP_BEGIN(nsGenericElement)
  NS_INTERFACE_MAP_ENTRY(nsIContent)
  NS_INTERFACE_MAP_ENTRY(nsINode)
  NS_INTERFACE_MAP_ENTRY(nsPIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOM3Node, new nsNode3Tearoff(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMNSElement, new nsNSElementTearoff(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMEventReceiver,
                                 nsDOMEventRTTearoff::Create(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMEventTarget,
                                 nsDOMEventRTTearoff::Create(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOM3EventTarget,
                                 nsDOMEventRTTearoff::Create(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMNSEventTarget,
                                 nsDOMEventRTTearoff::Create(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsISupportsWeakReference,
                                 new nsNodeSupportsWeakRefTearoff(this))
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContent)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsGenericElement)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsGenericElement, nsIContent)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS_WITH_DESTROY(nsGenericElement, 
							nsIContent,
							nsNodeUtils::LastRelease(this, PR_TRUE))

nsresult
nsGenericElement::PostQueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  nsIDocument *document = GetOwnerDoc();
  if (document) {
    return document->BindingManager()->GetBindingImplementation(this, aIID,
                                                                aInstancePtr);
  }

  return NS_NOINTERFACE;
}

//----------------------------------------------------------------------
nsresult
nsGenericElement::LeaveLink(nsPresContext* aPresContext)
{
  nsILinkHandler *handler = aPresContext->GetLinkHandler();
  if (!handler) {
    return NS_OK;
  }

  return handler->OnLeaveLink();
}

nsresult
nsGenericElement::TriggerLink(nsPresContext* aPresContext,
                              nsIURI* aLinkURI,
                              const nsAFlatString& aTargetSpec,
                              PRBool aClick,
                              PRBool aIsUserTriggered)
{
  NS_PRECONDITION(aLinkURI, "No link URI");
  nsresult rv = NS_OK;

  nsILinkHandler *handler = aPresContext->GetLinkHandler();
  if (!handler) return NS_OK;

  if (aClick) {
    nsresult proceed = NS_OK;
    // Check that this page is allowed to load this URI.
    nsCOMPtr<nsIScriptSecurityManager> securityManager = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      PRUint32 flag =
        aIsUserTriggered ?
        (PRUint32) nsIScriptSecurityManager::STANDARD :
        (PRUint32) nsIScriptSecurityManager::LOAD_IS_AUTOMATIC_DOCUMENT_REPLACEMENT;
      proceed =
        securityManager->CheckLoadURIWithPrincipal(NodePrincipal(), aLinkURI,
                                                   flag);
    }

    // Only pass off the click event if the script security manager
    // says it's ok.
    if (NS_SUCCEEDED(proceed))
      handler->OnLinkClick(this, aLinkURI, aTargetSpec.get());
  } else {
    handler->OnOverLink(this, aLinkURI, aTargetSpec.get());
  }
  return rv;
}

nsresult
nsGenericElement::AddScriptEventListener(nsIAtom* aEventName,
                                         const nsAString& aValue,
                                         PRBool aDefer)
{
  NS_PRECONDITION(aEventName, "Must have event name!");
  nsCOMPtr<nsISupports> target;
  PRBool defer = PR_TRUE;
  nsCOMPtr<nsIEventListenerManager> manager;

  nsresult rv = GetEventListenerManagerForAttr(getter_AddRefs(manager),
                                               getter_AddRefs(target),
                                               &defer);
  NS_ENSURE_SUCCESS(rv, rv);

  if (manager) {
    nsIDocument *ownerDoc = GetOwnerDoc();

    defer = defer && aDefer; // only defer if everyone agrees...

    PRUint32 lang = GetScriptTypeID();
    rv =
      manager->AddScriptEventListener(target, aEventName, aValue, lang, defer,
                                      !nsContentUtils::IsChromeDoc(ownerDoc));
  }

  return rv;
}


//----------------------------------------------------------------------

const nsAttrName*
nsGenericElement::InternalGetExistingAttrNameFromQName(const nsAString& aStr) const
{
  return mAttrsAndChildren.GetExistingAttrNameFromQName(
    NS_ConvertUTF16toUTF8(aStr));
}

nsresult
nsGenericElement::CopyInnerTo(nsGenericElement* aDst) const
{
  PRUint32 i, count = mAttrsAndChildren.AttrCount();
  for (i = 0; i < count; ++i) {
    const nsAttrName* name = mAttrsAndChildren.AttrNameAt(i);
    const nsAttrValue* value = mAttrsAndChildren.AttrAt(i);
    nsAutoString valStr;
    value->ToString(valStr);
    nsresult rv = aDst->SetAttr(name->NamespaceID(), name->LocalName(),
                                name->GetPrefix(), valStr, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
nsGenericElement::SetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                          nsIAtom* aPrefix, const nsAString& aValue,
                          PRBool aNotify)
{
  NS_ENSURE_ARG_POINTER(aName);
  NS_ASSERTION(aNamespaceID != kNameSpaceID_Unknown,
               "Don't call SetAttr with unknown namespace");

  nsIDocument* doc = GetCurrentDoc();
  if (kNameSpaceID_XLink == aNamespaceID && nsGkAtoms::href == aName) {
    // XLink URI(s) might be changing. Drop the link from the map. If it
    // is still style relevant it will be re-added by
    // nsStyleUtil::IsLink. Make sure to keep the style system
    // consistent so this remains true! In particular if the style system
    // were to get smarter and not restyling an XLink element if the href
    // doesn't change in a "significant" way, we'd need to do the same
    // significance check here.
    if (doc) {
      doc->ForgetLink(this);
    }
  }

  nsAutoString oldValue;
  PRBool modification = PR_FALSE;
  PRBool hasListeners = aNotify &&
    nsContentUtils::HasMutationListeners(this,
                                         NS_EVENT_BITS_MUTATION_ATTRMODIFIED);
  
  // If we have no listeners and aNotify is false, we are almost certainly
  // coming from the content sink and will almost certainly have no previous
  // value.  Even if we do, setting the value is cheap when we have no
  // listeners and don't plan to notify.  The check for aNotify here is an
  // optimization, the check for haveListeners is a correctness issue.
  if (hasListeners || aNotify) {
    nsAttrInfo info(GetAttrInfo(aNamespaceID, aName));
    if (info.mValue) {
      // Check whether the old value is the same as the new one.  Note that we
      // only need to actually _get_ the old value if we have listeners.
      PRBool valueMatches;
      if (hasListeners) {
        // Need to store the old value
        info.mValue->ToString(oldValue);
        valueMatches = aValue.Equals(oldValue);
      } else if (aNotify) {
        valueMatches = info.mValue->Equals(aValue, eCaseMatters);
      }
      if (valueMatches && aPrefix == info.mName->GetPrefix()) {
        return NS_OK;
      }
      modification = PR_TRUE;
    }
  }

  nsresult rv = BeforeSetAttr(aNamespaceID, aName, &aValue, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsAttrValue attrValue;
  if (!ParseAttribute(aNamespaceID, aName, aValue, attrValue)) {
    attrValue.SetTo(aValue);
  }

  rv = SetAttrAndNotify(aNamespaceID, aName, aPrefix, oldValue,
                        attrValue, modification, hasListeners, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  return AfterSetAttr(aNamespaceID, aName, &aValue, aNotify);
}
  
nsresult
nsGenericElement::SetAttrAndNotify(PRInt32 aNamespaceID,
                                   nsIAtom* aName,
                                   nsIAtom* aPrefix,
                                   const nsAString& aOldValue,
                                   nsAttrValue& aParsedValue,
                                   PRBool aModification,
                                   PRBool aFireMutation,
                                   PRBool aNotify)
{
  nsresult rv;
  PRUint8 modType = aModification ?
    NS_STATIC_CAST(PRUint8, nsIDOMMutationEvent::MODIFICATION) :
    NS_STATIC_CAST(PRUint8, nsIDOMMutationEvent::ADDITION);

  nsIDocument* document = GetCurrentDoc();
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, aNotify);
  if (aNotify && document) {
    document->AttributeWillChange(this, aNamespaceID, aName);
  }

  if (aNamespaceID == kNameSpaceID_None) {
    // XXXbz Perhaps we should push up the attribute mapping function
    // stuff to nsGenericElement?
    if (!IsAttributeMapped(aName) ||
        !SetMappedAttribute(document, aName, aParsedValue, &rv)) {
      rv = mAttrsAndChildren.SetAndTakeAttr(aName, aParsedValue);
    }
  }
  else {
    nsCOMPtr<nsINodeInfo> ni;
    rv = mNodeInfo->NodeInfoManager()->GetNodeInfo(aName, aPrefix,
                                                   aNamespaceID,
                                                   getter_AddRefs(ni));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = mAttrsAndChildren.SetAndTakeAttr(ni, aParsedValue);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  if (document) {
    nsXBLBinding *binding = document->BindingManager()->GetBinding(this);
    if (binding) {
      binding->AttributeChanged(aName, aNamespaceID, PR_FALSE, aNotify);
    }
  }

  if (aFireMutation) {
    nsMutationEvent mutation(PR_TRUE, NS_MUTATION_ATTRMODIFIED);

    nsAutoString attrName;
    aName->ToString(attrName);
    nsCOMPtr<nsIDOMAttr> attrNode;
    nsAutoString ns;
    nsContentUtils::NameSpaceManager()->GetNameSpaceURI(aNamespaceID, ns);
    GetAttributeNodeNS(ns, attrName, getter_AddRefs(attrNode));
    mutation.mRelatedNode = attrNode;

    mutation.mAttrName = aName;
    nsAutoString newValue;
    GetAttr(aNamespaceID, aName, newValue);
    if (!newValue.IsEmpty()) {
      mutation.mNewAttrValue = do_GetAtom(newValue);
    }
    if (!aOldValue.IsEmpty()) {
      mutation.mPrevAttrValue = do_GetAtom(aOldValue);
    }
    mutation.mAttrChange = modType;
    mozAutoSubtreeModified subtree(GetOwnerDoc(), this);
    nsEventDispatcher::Dispatch(this, nsnull, &mutation);
  }

  if (aNotify) {
    nsNodeUtils::AttributeChanged(this, aNamespaceID, aName, modType);
  }
  
  if (aNamespaceID == kNameSpaceID_XMLEvents && 
      aName == nsGkAtoms::event && mNodeInfo->GetDocument()) {
    mNodeInfo->GetDocument()->AddXMLEventsContent(this);
  }

  return NS_OK;
}

PRBool
nsGenericElement::ParseAttribute(PRInt32 aNamespaceID,
                                 nsIAtom* aAttribute,
                                 const nsAString& aValue,
                                 nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None &&
      aAttribute == GetIDAttributeName() && !aValue.IsEmpty()) {
    // Store id as an atom.  id="" means that the element has no id,
    // not that it has an emptystring as the id.
    aResult.ParseAtom(aValue);
    return PR_TRUE;
  }

  return PR_FALSE;
}

PRBool
nsGenericElement::SetMappedAttribute(nsIDocument* aDocument,
                                     nsIAtom* aName,
                                     nsAttrValue& aValue,
                                     nsresult* aRetval)
{
  *aRetval = NS_OK;
  return PR_FALSE;
}

nsresult
nsGenericElement::GetEventListenerManagerForAttr(nsIEventListenerManager** aManager,
                                                 nsISupports** aTarget,
                                                 PRBool* aDefer)
{
  nsresult rv = GetListenerManager(PR_TRUE, aManager);
  if (NS_SUCCEEDED(rv)) {
    NS_ADDREF(*aTarget = NS_STATIC_CAST(nsIContent*, this));
  }
  *aDefer = PR_TRUE;
  return rv;
}

nsGenericElement::nsAttrInfo
nsGenericElement::GetAttrInfo(PRInt32 aNamespaceID, nsIAtom* aName) const
{
  NS_ASSERTION(nsnull != aName, "must have attribute name");
  NS_ASSERTION(aNamespaceID != kNameSpaceID_Unknown,
               "must have a real namespace ID!");

  PRInt32 index = mAttrsAndChildren.IndexOfAttr(aName, aNamespaceID);
  if (index >= 0) {
    return nsAttrInfo(mAttrsAndChildren.AttrNameAt(index),
                      mAttrsAndChildren.AttrAt(index));
  }

  return nsAttrInfo(nsnull, nsnull);
}
  

PRBool
nsGenericElement::GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                          nsAString& aResult) const
{
  NS_ASSERTION(nsnull != aName, "must have attribute name");
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown,
               "must have a real namespace ID!");

  const nsAttrValue* val = mAttrsAndChildren.GetAttr(aName, aNameSpaceID);
  if (!val) {
    // Since we are returning a success code we'd better do
    // something about the out parameters (someone may have
    // given us a non-empty string).
    aResult.Truncate();
    
    return PR_FALSE;
  }

  val->ToString(aResult);

  return PR_TRUE;
}

PRBool
nsGenericElement::HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const
{
  NS_ASSERTION(nsnull != aName, "must have attribute name");
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown,
               "must have a real namespace ID!");

  return mAttrsAndChildren.IndexOfAttr(aName, aNameSpaceID) >= 0;
}

PRBool
nsGenericElement::AttrValueIs(PRInt32 aNameSpaceID,
                              nsIAtom* aName,
                              const nsAString& aValue,
                              nsCaseTreatment aCaseSensitive) const
{
  NS_ASSERTION(aName, "Must have attr name");
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown, "Must have namespace");

  const nsAttrValue* val = mAttrsAndChildren.GetAttr(aName, aNameSpaceID);
  return val && val->Equals(aValue, aCaseSensitive);
}

PRBool
nsGenericElement::AttrValueIs(PRInt32 aNameSpaceID,
                              nsIAtom* aName,
                              nsIAtom* aValue,
                              nsCaseTreatment aCaseSensitive) const
{
  NS_ASSERTION(aName, "Must have attr name");
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown, "Must have namespace");
  NS_ASSERTION(aValue, "Null value atom");

  const nsAttrValue* val = mAttrsAndChildren.GetAttr(aName, aNameSpaceID);
  return val && val->Equals(aValue, aCaseSensitive);
}

PRInt32
nsGenericElement::FindAttrValueIn(PRInt32 aNameSpaceID,
                                  nsIAtom* aName,
                                  AttrValuesArray* aValues,
                                  nsCaseTreatment aCaseSensitive) const
{
  NS_ASSERTION(aName, "Must have attr name");
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown, "Must have namespace");
  NS_ASSERTION(aValues, "Null value array");
  
  const nsAttrValue* val = mAttrsAndChildren.GetAttr(aName, aNameSpaceID);
  if (val) {
    for (PRInt32 i = 0; aValues[i]; ++i) {
      if (val->Equals(*aValues[i], aCaseSensitive)) {
        return i;
      }
    }
    return ATTR_VALUE_NO_MATCH;
  }
  return ATTR_MISSING;
}

nsresult
nsGenericElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                            PRBool aNotify)
{
  NS_ASSERTION(nsnull != aName, "must have attribute name");

  PRInt32 index = mAttrsAndChildren.IndexOfAttr(aName, aNameSpaceID);
  if (index < 0) {
    return NS_OK;
  }

  nsIDocument *document = GetCurrentDoc();    
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, aNotify);
  if (document) {
    if (kNameSpaceID_XLink == aNameSpaceID && nsGkAtoms::href == aName) {
      // XLink URI might be changing.
      document->ForgetLink(this);
    }

    if (aNotify) {
      document->AttributeWillChange(this, aNameSpaceID, aName);
    }
  }

  PRBool hasMutationListeners = aNotify &&
    nsContentUtils::HasMutationListeners(this,
                                         NS_EVENT_BITS_MUTATION_ATTRMODIFIED);

  // Grab the attr node if needed before we remove it from the attr map
  nsCOMPtr<nsIDOMAttr> attrNode;
  if (hasMutationListeners) {
    nsAutoString attrName;
    aName->ToString(attrName);
    nsAutoString ns;
    nsContentUtils::NameSpaceManager()->GetNameSpaceURI(aNameSpaceID, ns);
    GetAttributeNodeNS(ns, attrName, getter_AddRefs(attrNode));
  }

  // Clear binding to nsIDOMNamedNodeMap
  nsDOMSlots *slots = GetExistingDOMSlots();
  if (slots && slots->mAttributeMap) {
    slots->mAttributeMap->DropAttribute(aNameSpaceID, aName);
  }

  nsAttrValue oldValue;
  nsresult rv = mAttrsAndChildren.RemoveAttrAt(index, oldValue);
  NS_ENSURE_SUCCESS(rv, rv);

  if (document) {
    nsXBLBinding *binding = document->BindingManager()->GetBinding(this);
    if (binding)
      binding->AttributeChanged(aName, aNameSpaceID, PR_TRUE, aNotify);
  }

  if (aNotify) {
    nsNodeUtils::AttributeChanged(this, aNameSpaceID, aName,
                                  nsIDOMMutationEvent::REMOVAL);
  }

  if (hasMutationListeners) {
    nsCOMPtr<nsIDOMEventTarget> node =
      do_QueryInterface(NS_STATIC_CAST(nsIContent *, this));
    nsMutationEvent mutation(PR_TRUE, NS_MUTATION_ATTRMODIFIED);

    mutation.mRelatedNode = attrNode;
    mutation.mAttrName = aName;

    nsAutoString value;
    oldValue.ToString(value);
    if (!value.IsEmpty())
      mutation.mPrevAttrValue = do_GetAtom(value);
    mutation.mAttrChange = nsIDOMMutationEvent::REMOVAL;

    mozAutoSubtreeModified subtree(GetOwnerDoc(), this);
    nsEventDispatcher::Dispatch(this, nsnull, &mutation);
  }

  return NS_OK;
}

const nsAttrName*
nsGenericElement::GetAttrNameAt(PRUint32 aIndex) const
{
  return mAttrsAndChildren.GetSafeAttrNameAt(aIndex);
}

PRUint32
nsGenericElement::GetAttrCount() const
{
  return mAttrsAndChildren.AttrCount();
}

const nsTextFragment*
nsGenericElement::GetText()
{
  return nsnull;
}

PRUint32
nsGenericElement::TextLength()
{
  // We can remove this assertion if it turns out to be useful to be able
  // to depend on this returning 0
  NS_NOTREACHED("called nsGenericElement::TextLength");

  return 0;
}

nsresult
nsGenericElement::SetText(const PRUnichar* aBuffer, PRUint32 aLength,
                          PRBool aNotify)
{
  NS_ERROR("called nsGenericElement::SetText");

  return NS_ERROR_FAILURE;
}

nsresult
nsGenericElement::AppendText(const PRUnichar* aBuffer, PRUint32 aLength,
                             PRBool aNotify)
{
  NS_ERROR("called nsGenericElement::AppendText");

  return NS_ERROR_FAILURE;
}

PRBool
nsGenericElement::TextIsOnlyWhitespace()
{
  return PR_FALSE;
}

void
nsGenericElement::AppendTextTo(nsAString& aResult)
{
  // We can remove this assertion if it turns out to be useful to be able
  // to depend on this appending nothing.
  NS_NOTREACHED("called nsGenericElement::TextLength");
}

#ifdef DEBUG
void
nsGenericElement::ListAttributes(FILE* out) const
{
  PRUint32 index, count = mAttrsAndChildren.AttrCount();
  for (index = 0; index < count; index++) {
    nsAutoString buffer;

    // name
    mAttrsAndChildren.AttrNameAt(index)->GetQualifiedName(buffer);

    // value
    buffer.AppendLiteral("=\"");
    nsAutoString value;
    mAttrsAndChildren.AttrAt(index)->ToString(value);
    for (int i = value.Length(); i >= 0; --i) {
      if (value[i] == PRUnichar('"'))
        value.Insert(PRUnichar('\\'), PRUint32(i));
    }
    buffer.Append(value);
    buffer.AppendLiteral("\"");

    fputs(" ", out);
    fputs(NS_LossyConvertUTF16toASCII(buffer).get(), out);
  }
}

void
nsGenericElement::List(FILE* out, PRInt32 aIndent,
                       const nsCString& aPrefix) const
{
  PRInt32 indent;
  for (indent = aIndent; --indent >= 0; ) fputs("  ", out);

  fputs(aPrefix.get(), out);

  nsAutoString buf;
  mNodeInfo->GetQualifiedName(buf);
  fputs(NS_LossyConvertUTF16toASCII(buf).get(), out);

  fprintf(out, "@%p", (void *)this);

  ListAttributes(out);

  fprintf(out, " intrinsicstate=[%08x]", IntrinsicState());
  fprintf(out, " refcount=%d<", mRefCnt.get());

  PRUint32 i, length = GetChildCount();
  if (length > 0) {
    fputs("\n", out);

    for (i = 0; i < length; ++i) {
      nsIContent *kid = GetChildAt(i);
      kid->List(out, aIndent + 1);
    }

    for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
  }

  fputs(">\n", out);
  
  nsGenericElement* nonConstThis = NS_CONST_CAST(nsGenericElement*, this);

  // XXX sXBL/XBL2 issue! Owner or current document?
  nsIDocument *document = GetOwnerDoc();
  if (document) {
    // Note: not listing nsIAnonymousContentCreator-created content...

    nsBindingManager* bindingManager = document->BindingManager();
    nsCOMPtr<nsIDOMNodeList> anonymousChildren;
    bindingManager->GetAnonymousNodesFor(nonConstThis,
                                         getter_AddRefs(anonymousChildren));

    if (anonymousChildren) {
      anonymousChildren->GetLength(&length);
      if (length > 0) {
        for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
        fputs("anonymous-children<\n", out);

        for (i = 0; i < length; ++i) {
          nsCOMPtr<nsIDOMNode> node;
          anonymousChildren->Item(i, getter_AddRefs(node));
          nsCOMPtr<nsIContent> child = do_QueryInterface(node);
          child->List(out, aIndent + 1);
        }

        for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
        fputs(">\n", out);
      }
    }

    if (bindingManager->HasContentListFor(nonConstThis)) {
      nsCOMPtr<nsIDOMNodeList> contentList;
      bindingManager->GetContentListFor(nonConstThis,
                                        getter_AddRefs(contentList));

      NS_ASSERTION(contentList != nsnull, "oops, binding manager lied");

      contentList->GetLength(&length);
      if (length > 0) {
        for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
        fputs("content-list<\n", out);

        for (i = 0; i < length; ++i) {
          nsCOMPtr<nsIDOMNode> node;
          contentList->Item(i, getter_AddRefs(node));
          nsCOMPtr<nsIContent> child = do_QueryInterface(node);
          child->List(out, aIndent + 1);
        }

        for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
        fputs(">\n", out);
      }
    }
  }
}

void
nsGenericElement::DumpContent(FILE* out, PRInt32 aIndent,
                              PRBool aDumpAll) const
{
  PRInt32 indent;
  for (indent = aIndent; --indent >= 0; ) fputs("  ", out);

  nsAutoString buf;
  mNodeInfo->GetQualifiedName(buf);
  fputs("<", out);
  fputs(NS_LossyConvertUTF16toASCII(buf).get(), out);

  if(aDumpAll) ListAttributes(out);

  fputs(">", out);

  if(aIndent) fputs("\n", out);

  PRInt32 index, kids = GetChildCount();
  for (index = 0; index < kids; index++) {
    nsIContent *kid = GetChildAt(index);
    PRInt32 indent = aIndent ? aIndent + 1 : 0;
    kid->DumpContent(out, indent, aDumpAll);
  }
  for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
  fputs("</", out);
  fputs(NS_LossyConvertUTF16toASCII(buf).get(), out);
  fputs(">", out);

  if(aIndent) fputs("\n", out);
}
#endif

PRUint32
nsGenericElement::GetChildCount() const
{
  return mAttrsAndChildren.ChildCount();
}

nsIContent *
nsGenericElement::GetChildAt(PRUint32 aIndex) const
{
  return mAttrsAndChildren.GetSafeChildAt(aIndex);
}

PRInt32
nsGenericElement::IndexOf(nsINode* aPossibleChild) const
{
  return mAttrsAndChildren.IndexOfChild(aPossibleChild);
}

nsINode::nsSlots*
nsGenericElement::CreateSlots()
{
  return new nsDOMSlots(mFlagsOrSlots);
}

nsresult
nsGenericElement::PostHandleEventForLinks(nsEventChainPostVisitor& aVisitor)
{
  // Optimisation: return early if this event doesn't interest us.
  // IMPORTANT: this switch and the switch below it must be kept in sync!
  switch (aVisitor.mEvent->message) {
  case NS_MOUSE_BUTTON_DOWN:
  case NS_MOUSE_CLICK:
  case NS_UI_ACTIVATE:
  case NS_KEY_PRESS:
  case NS_MOUSE_ENTER_SYNTH:
  case NS_FOCUS_CONTENT:
  case NS_MOUSE_EXIT_SYNTH:
  case NS_BLUR_CONTENT:
    break;
  default:
    return NS_OK;
  }

  if (aVisitor.mEventStatus == nsEventStatus_eConsumeNoDefault ||
      !NS_IS_TRUSTED_EVENT(aVisitor.mEvent) ||
      !aVisitor.mPresContext) {
    return NS_OK;
  }

  // Make sure we actually are a link before continuing
  nsCOMPtr<nsIURI> absURI;
  if (!IsLink(getter_AddRefs(absURI))) {
    return NS_OK;
  }

  nsresult rv = NS_OK;

  switch (aVisitor.mEvent->message) {
  case NS_MOUSE_BUTTON_DOWN:
    {
      if (aVisitor.mEvent->eventStructType == NS_MOUSE_EVENT &&
          NS_STATIC_CAST(nsMouseEvent*, aVisitor.mEvent)->button ==
          nsMouseEvent::eLeftButton) {
        // don't make the link grab the focus if there is no link handler
        nsILinkHandler *handler = aVisitor.mPresContext->GetLinkHandler();
        nsIDocument *document = GetCurrentDoc();
        if (handler && document && ShouldFocus(this)) {
          // If the window is not active, do not allow the focus to bring the
          // window to the front. We update the focus controller, but do nothing
          // else.
          nsPIDOMWindow *win = document->GetWindow();
          if (win) {
            nsIFocusController *focusController =
              win->GetRootFocusController();
            if (focusController) {
              PRBool isActive = PR_FALSE;
              focusController->GetActive(&isActive);
              if (!isActive) {
                nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(this);
                if(domElement)
                  focusController->SetFocusedElement(domElement);
                break;
              }
            }
          }
  
          aVisitor.mPresContext->EventStateManager()->
            SetContentState(this, NS_EVENT_STATE_ACTIVE | NS_EVENT_STATE_FOCUS);
        }
      }
    }
    break;

  case NS_MOUSE_CLICK:
    if (NS_IS_MOUSE_LEFT_CLICK(aVisitor.mEvent)) {
      nsInputEvent* inputEvent = NS_STATIC_CAST(nsInputEvent*, aVisitor.mEvent);
      if (inputEvent->isControl || inputEvent->isMeta ||
          inputEvent->isAlt ||inputEvent->isShift) {
        break;
      }

      // The default action is simply to dispatch DOMActivate
      nsIPresShell *shell = aVisitor.mPresContext->GetPresShell();
      if (shell) {
        // single-click
        nsEventStatus status = nsEventStatus_eIgnore;
        nsUIEvent actEvent(NS_IS_TRUSTED_EVENT(aVisitor.mEvent),
                           NS_UI_ACTIVATE, 1);

        rv = shell->HandleDOMEventWithTarget(this, &actEvent, &status);
      }
    }
    break;

  case NS_UI_ACTIVATE:
    {
      nsAutoString target;
      GetLinkTarget(target);
      rv = TriggerLink(aVisitor.mPresContext, absURI, target, PR_TRUE, PR_TRUE);
    }
    break;

  case NS_KEY_PRESS:
    {
      if (aVisitor.mEvent->eventStructType == NS_KEY_EVENT) {
        nsKeyEvent* keyEvent = NS_STATIC_CAST(nsKeyEvent*, aVisitor.mEvent);
        if (keyEvent->keyCode == NS_VK_RETURN) {
          nsEventStatus status = nsEventStatus_eIgnore;
          rv = DispatchClickEvent(aVisitor.mPresContext, keyEvent, this,
                                  PR_FALSE, &status);
          if (NS_SUCCEEDED(rv)) {
            aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
          }
        }
      }
    }
    break;

  // Set the status bar the same for focus and mouseover
  case NS_MOUSE_ENTER_SYNTH:
    aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
    // FALL THROUGH
  case NS_FOCUS_CONTENT:
    {
      nsAutoString target;
      GetLinkTarget(target);
      rv = TriggerLink(aVisitor.mPresContext, absURI, target, PR_FALSE, PR_TRUE);
    }
    break;

  case NS_MOUSE_EXIT_SYNTH:
    aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
    rv = LeaveLink(aVisitor.mPresContext);
    break;

  case NS_BLUR_CONTENT:
    rv = LeaveLink(aVisitor.mPresContext);
    break;

  default:
    // switch not in sync with the optimization switch earlier in this function
    NS_NOTREACHED("switch statements not in sync");
    return NS_ERROR_UNEXPECTED;
  }

  return rv;
}

void
nsGenericElement::GetLinkTarget(nsAString& aTarget)
{
  aTarget.Truncate();
}

