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
#include "nsGenericElement.h"

#include "nsDOMAttribute.h"
#include "nsDOMAttributeMap.h"
#include "nsIAtom.h"
#include "nsINodeInfo.h"
#include "nsIDocument.h"
#include "nsIDocumentEncoder.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMRange.h"
#include "nsIDOMText.h"
#include "nsIDOMEventReceiver.h"
#include "nsITextContent.h"
#include "nsIContentIterator.h"
#include "nsRange.h"
#include "nsIEventListenerManager.h"
#include "nsILinkHandler.h"
#include "nsIScriptGlobalObject.h"
#include "nsISupportsArray.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsStyleConsts.h"
#include "nsIView.h"
#include "nsIViewManager.h"
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

#include "nsIBindingManager.h"
#include "nsXBLBinding.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMViewCSS.h"
#include "nsIXBLService.h"
#include "nsPIDOMWindow.h"
#include "nsIBoxObject.h"
#include "nsPIBoxObject.h"
#include "nsIDOMNSDocument.h"

#include "nsLayoutAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsContentUtils.h"
#include "nsIJSContextStack.h"

#include "nsIServiceManager.h"
#include "nsIDOMEventListener.h"

#include "nsIWebNavigation.h"
#include "nsIBaseWindow.h"

#include "jsapi.h"

#include "nsIDOMXPathEvaluator.h"
#include "nsNodeInfoManager.h"
#include "nsICategoryManager.h"
#include "nsIDOMNSFeatureFactory.h"

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

PLDHashTable nsGenericElement::sRangeListsHash;
PLDHashTable nsGenericElement::sEventListenerManagersHash;
PRInt32 nsIContent::sTabFocusModel = eTabFocus_any;
PRBool nsIContent::sTabFocusModelAppliesToXUL = PR_FALSE;
nsresult NS_NewContentIterator(nsIContentIterator** aInstancePtrResult);
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
  if (mContent) {
    *aLength = mContent->GetChildCount();
  } else {
    *aLength = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsChildContentList::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  if (mContent) {
    nsIContent *content = mContent->GetChildAt(aIndex);
    if (content) {
      return CallQueryInterface(content, aReturn);
    }
  }

  *aReturn = nsnull;

  return NS_OK;
}

void
nsChildContentList::DropReference()
{
  mContent = nsnull;
}

NS_INTERFACE_MAP_BEGIN(nsNode3Tearoff)
  NS_INTERFACE_MAP_ENTRY(nsIDOM3Node)
NS_INTERFACE_MAP_END_AGGREGATED(mContent)

NS_IMPL_ADDREF(nsNode3Tearoff)
NS_IMPL_RELEASE(nsNode3Tearoff)

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

  return GetTextContent(mContent, aTextContent);
}

// static
nsresult
nsNode3Tearoff::GetTextContent(nsIContent *aContent,
                               nsAString &aTextContent)
{
  NS_ENSURE_ARG_POINTER(aContent);

  nsCOMPtr<nsIContentIterator> iter;
  NS_NewContentIterator(getter_AddRefs(iter));
  iter->Init(aContent);

  aTextContent.Truncate();
  while (!iter->IsDone()) {
    nsIContent *content = iter->GetCurrentNode();
    if (content->IsContentOfType(nsIContent::eTEXT)) {
      nsCOMPtr<nsITextContent> textContent(do_QueryInterface(iter->GetCurrentNode()));
      if (textContent)
        textContent->AppendTextTo(aTextContent);
    }
    iter->Next();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNode3Tearoff::SetTextContent(const nsAString &aTextContent)
{
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

  return SetTextContent(mContent, aTextContent);
}

// static
nsresult
nsNode3Tearoff::SetTextContent(nsIContent* aContent,
                               const nsAString &aTextContent)
{
  PRUint32 childCount = aContent->GetChildCount();

  // i is unsigned, so i >= is always true
  for (PRUint32 i = childCount; i-- != 0; ) {
    aContent->RemoveChildAt(i, PR_TRUE);
  }

  if (!aTextContent.IsEmpty()) {
    nsCOMPtr<nsITextContent> textContent;
    nsresult rv = NS_NewTextNode(getter_AddRefs(textContent));
    NS_ENSURE_SUCCESS(rv, rv);

    textContent->SetText(aTextContent, PR_TRUE);

    aContent->AppendChildTo(textContent, PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNode3Tearoff::CompareDocumentPosition(nsIDOMNode* aOther,
                                        PRUint16* aReturn)
{
  NS_ENSURE_ARG_POINTER(aOther);
  NS_PRECONDITION(aReturn, "Must have an out parameter");

  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mContent));
  if (node == aOther) {
    // If the two nodes being compared are the same node,
    // then no flags are set on the return.
    *aReturn = 0;
    return NS_OK;
  }

  PRUint16 mask = 0;

  // If the other node is an attribute, document, or document fragment,
  // we can find the position easier by comparing this node relative to
  // the other node, and then reversing positions.
  PRUint16 otherType = 0;
  aOther->GetNodeType(&otherType);
  if (otherType == nsIDOMNode::ATTRIBUTE_NODE ||
      otherType == nsIDOMNode::DOCUMENT_NODE  ||
      otherType == nsIDOMNode::DOCUMENT_FRAGMENT_NODE) {
    PRUint16 otherMask = 0;
    nsCOMPtr<nsIDOM3Node> other(do_QueryInterface(aOther));
    other->CompareDocumentPosition(node, &otherMask);

    *aReturn = nsContentUtils::ReverseDocumentPosition(otherMask);
    return NS_OK;
  }

#ifdef DEBUG
  {
    PRUint16 nodeType = 0;
    node->GetNodeType(&nodeType);

    if (nodeType == nsIDOMNode::ENTITY_NODE ||
        nodeType == nsIDOMNode::NOTATION_NODE) {
      NS_NOTYETIMPLEMENTED("Entities and Notations are not fully supported yet");
    }
    else {
      NS_ASSERTION((nodeType == nsIDOMNode::ELEMENT_NODE ||
                    nodeType == nsIDOMNode::TEXT_NODE ||
                    nodeType == nsIDOMNode::CDATA_SECTION_NODE ||
                    nodeType == nsIDOMNode::ENTITY_REFERENCE_NODE ||
                    nodeType == nsIDOMNode::PROCESSING_INSTRUCTION_NODE ||
                    nodeType == nsIDOMNode::COMMENT_NODE),
                   "Invalid node type!");
    }
  }
#endif

  mask |= nsContentUtils::ComparePositionWithAncestors(node, aOther);

  *aReturn = mask;
  return NS_OK;
}

NS_IMETHODIMP
nsNode3Tearoff::IsSameNode(nsIDOMNode* aOther,
                           PRBool* aReturn)
{
  PRBool sameNode = PR_FALSE;

  nsCOMPtr<nsIContent> other(do_QueryInterface(aOther));
  if (mContent == other) {
    sameNode = PR_TRUE;
  }

  *aReturn = sameNode;
  return NS_OK;
}

NS_IMETHODIMP
nsNode3Tearoff::IsEqualNode(nsIDOMNode* aOther, PRBool* aReturn)
{
  NS_NOTYETIMPLEMENTED("nsNode3Tearoff::IsEqualNode()");

  return NS_ERROR_NOT_IMPLEMENTED;
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
                            nsIVariant** aReturn)
{
  NS_NOTYETIMPLEMENTED("nsNode3Tearoff::SetUserData()");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNode3Tearoff::GetUserData(const nsAString& aKey,
                            nsIVariant** aReturn)
{
  NS_NOTYETIMPLEMENTED("nsNode3Tearoff::GetUserData()");

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNode3Tearoff::LookupPrefix(const nsAString& aNamespaceURI,
                             nsAString& aPrefix)
{
  SetDOMStringToNull(aPrefix);

  // XXX Waiting for DOM spec to list error codes.

  // Get the namespace id for the URI
  PRInt32 namespaceId =
    nsContentUtils::NameSpaceManager()->GetNameSpaceID(aNamespaceURI);
  if (namespaceId == kNameSpaceID_Unknown) {
    return NS_OK;
  }

  nsCOMPtr<nsIAtom> name, prefix;
  PRInt32 namespace_id;
  nsAutoString ns;

  // Trace up the content parent chain looking for the namespace
  // declaration that defines the aNamespaceURI namespace. Once found,
  // return the prefix (i.e. the attribute localName).
  for (nsIContent* content = mContent; content;
       content = content->GetParent()) {
    PRUint32 attrCount = content->GetAttrCount();

    for (PRUint32 i = 0; i < attrCount; ++i) {
      content->GetAttrNameAt(i, &namespace_id, getter_AddRefs(name),
                             getter_AddRefs(prefix));

      if (namespace_id == kNameSpaceID_XMLNS) {
        nsresult rv = content->GetAttr(namespace_id, name, ns);

        if (rv == NS_CONTENT_ATTR_HAS_VALUE && ns.Equals(aNamespaceURI)) {
          name->ToString(aPrefix);

          return NS_OK;
        }
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

NS_INTERFACE_MAP_BEGIN(nsDOMEventRTTearoff)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventReceiver)
  NS_INTERFACE_MAP_ENTRY(nsIDOM3EventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSEventTarget)
NS_INTERFACE_MAP_END_AGGREGATED(mContent)

NS_IMPL_ADDREF(nsDOMEventRTTearoff)
NS_IMPL_RELEASE_WITH_DESTROY(nsDOMEventRTTearoff, LastRelease())

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

    mContent = nsnull;

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
  nsresult rv = mContent->GetListenerManager(getter_AddRefs(listener_manager));
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(listener_manager, aReceiver);
}

nsresult
nsDOMEventRTTearoff::GetDOM3EventTarget(nsIDOM3EventTarget **aTarget)
{
  nsCOMPtr<nsIEventListenerManager> listener_manager;
  nsresult rv = mContent->GetListenerManager(getter_AddRefs(listener_manager));
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
nsDOMEventRTTearoff::GetListenerManager(nsIEventListenerManager** aResult)
{
  return mContent->GetListenerManager(aResult);
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
  GetListenerManager(getter_AddRefs(manager));

  if (!manager) {
    return NS_ERROR_FAILURE;
  }

  return manager->GetSystemEventGroupLM(aGroup);
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
  nsresult rv = mContent->GetListenerManager(getter_AddRefs(listener_manager));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

  if (aWantsUntrusted) {
    flags |= NS_PRIV_EVENT_UNTRUSTED_PERMITTED;
  }

  return listener_manager->AddEventListenerByType(aListener, aType, flags,
                                                  nsnull);
}

//----------------------------------------------------------------------

nsDOMSlots::nsDOMSlots(PtrBits aFlags)
  : mFlags(aFlags & ~GENERIC_ELEMENT_CONTENT_ID_MASK),
    mBindingParent(nsnull),
    mContentID(aFlags >> GENERIC_ELEMENT_CONTENT_ID_BITS_OFFSET)
{
}

nsDOMSlots::~nsDOMSlots()
{
  if (mChildNodes) {
    mChildNodes->DropReference();
  }

  if (mStyle) {
    mStyle->DropReference();
  }

  if (mAttributeMap) {
    mAttributeMap->DropReference();
  }
}

PRBool
nsDOMSlots::IsEmpty()
{
  return (!mChildNodes && !mStyle && !mAttributeMap && !mBindingParent &&
          mContentID < GENERIC_ELEMENT_CONTENT_ID_MAX_VALUE);
}

PR_STATIC_CALLBACK(void)
NopClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  // Do nothing
}

// static
void
nsGenericElement::Shutdown()
{
  nsDOMEventRTTearoff::Shutdown();

  if (sRangeListsHash.ops) {
    NS_ASSERTION(sRangeListsHash.entryCount == 0,
                 "nsGenericElement's range hash not empty at shutdown!");

    // We're already being shut down and if there are entries left in
    // this hash at this point it means we leaked nsGenericElements or
    // nsGenericDOMDataNodes. Since we're already partly through the
    // shutdown process it's too late to release what's held on to by
    // this hash (since the teardown code relies on some things being
    // around that aren't around any more) so we rather leak what's
    // already leaked in stead of crashing trying to release what
    // should've been released much earlier on.

    // Copy the ops out of the hash table
    PLDHashTableOps hash_table_ops = *sRangeListsHash.ops;

    // Set the clearEntry hook to be a nop
    hash_table_ops.clearEntry = NopClearEntry;

    // Set the ops in the hash table to be the new ops
    sRangeListsHash.ops = &hash_table_ops;

    PL_DHashTableFinish(&sRangeListsHash);

    sRangeListsHash.ops = nsnull;
  }

  if (sEventListenerManagersHash.ops) {
    NS_ASSERTION(sEventListenerManagersHash.entryCount == 0,
                 "nsGenericElement's event listener manager hash not empty "
                 "at shutdown!");

    // See comment above.

    // Copy the ops out of the hash table
    PLDHashTableOps hash_table_ops = *sEventListenerManagersHash.ops;

    // Set the clearEntry hook to be a nop
    hash_table_ops.clearEntry = NopClearEntry;

    // Set the ops in the hash table to be the new ops
    sEventListenerManagersHash.ops = &hash_table_ops;

    PL_DHashTableFinish(&sEventListenerManagersHash);

    sEventListenerManagersHash.ops = nsnull;
  }
}

nsGenericElement::nsGenericElement(nsINodeInfo *aNodeInfo)
  : mNodeInfo(aNodeInfo),
    mFlagsOrSlots(GENERIC_ELEMENT_DOESNT_HAVE_DOMSLOTS)
{
  NS_ASSERTION(mNodeInfo, "No nsINodeInfo passed to nsGenericElement, "
               "PREPARE TO CRASH!!!");
}

nsGenericElement::~nsGenericElement()
{
  NS_PRECONDITION(!IsInDoc(),
                  "Please remove this from the document properly");
  
  // pop any enclosed ranges out
  // nsRange::OwnerGone(mContent); not used for now

  if (HasRangeList()) {
#ifdef DEBUG
    {
      RangeListMapEntry *entry =
        NS_STATIC_CAST(RangeListMapEntry *,
                       PL_DHashTableOperate(&sRangeListsHash, this,
                                            PL_DHASH_LOOKUP));

      if (PL_DHASH_ENTRY_IS_FREE(entry)) {
        NS_ERROR("Huh, our bit says we have a range list, but there's nothing "
                 "in the hash!?!!");
      }
    }
#endif

    PL_DHashTableOperate(&sRangeListsHash, this, PL_DHASH_REMOVE);
  }

  if (HasEventListenerManager()) {
#ifdef DEBUG
    {
      EventListenerManagerMapEntry *entry =
        NS_STATIC_CAST(EventListenerManagerMapEntry *,
                       PL_DHashTableOperate(&sEventListenerManagersHash, this,
                                            PL_DHASH_LOOKUP));

      if (PL_DHASH_ENTRY_IS_FREE(entry)) {
        NS_ERROR("Huh, our bit says we have a listener manager list, "
                 "but there's nothing in the hash!?!!");
      }
    }
#endif

    PL_DHashTableOperate(&sEventListenerManagersHash, this, PL_DHASH_REMOVE);
  }

  if (HasDOMSlots()) {
    nsDOMSlots *slots = GetDOMSlots();

    delete slots;
  }

  // No calling GetFlags() beyond this point...
}

PR_STATIC_CALLBACK(PRBool)
RangeListHashInitEntry(PLDHashTable *table, PLDHashEntryHdr *entry,
                       const void *key)
{
  // Initialize the entry with placement new
  new (entry) RangeListMapEntry(key);
  return PR_TRUE;
}

PR_STATIC_CALLBACK(void)
RangeListHashClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  RangeListMapEntry *r = NS_STATIC_CAST(RangeListMapEntry *, entry);

  // Let the RangeListMapEntry clean itself up...
  r->~RangeListMapEntry();
}

PR_STATIC_CALLBACK(PRBool)
EventListenerManagerHashInitEntry(PLDHashTable *table, PLDHashEntryHdr *entry,
                                  const void *key)
{
  // Initialize the entry with placement new
  new (entry) EventListenerManagerMapEntry(key);
  return PR_TRUE;
}

PR_STATIC_CALLBACK(void)
EventListenerManagerHashClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  EventListenerManagerMapEntry *lm =
    NS_STATIC_CAST(EventListenerManagerMapEntry *, entry);

  // Let the EventListenerManagerMapEntry clean itself up...
  lm->~EventListenerManagerMapEntry();
}


// static
nsresult
nsGenericElement::InitHashes()
{
  NS_ASSERTION(sizeof(PtrBits) == sizeof(void *),
               "Eeek! You'll need to adjust the size of PtrBits to the size "
               "of a pointer on your platform.");

  if (!sRangeListsHash.ops) {
    static PLDHashTableOps hash_table_ops =
    {
      PL_DHashAllocTable,
      PL_DHashFreeTable,
      PL_DHashGetKeyStub,
      PL_DHashVoidPtrKeyStub,
      PL_DHashMatchEntryStub,
      PL_DHashMoveEntryStub,
      RangeListHashClearEntry,
      PL_DHashFinalizeStub,
      RangeListHashInitEntry
    };

    if (!PL_DHashTableInit(&sRangeListsHash, &hash_table_ops, nsnull,
                           sizeof(RangeListMapEntry), 16)) {
      sRangeListsHash.ops = nsnull;

      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  if (!sEventListenerManagersHash.ops) {
    static PLDHashTableOps hash_table_ops =
    {
      PL_DHashAllocTable,
      PL_DHashFreeTable,
      PL_DHashGetKeyStub,
      PL_DHashVoidPtrKeyStub,
      PL_DHashMatchEntryStub,
      PL_DHashMoveEntryStub,
      EventListenerManagerHashClearEntry,
      PL_DHashFinalizeStub,
      EventListenerManagerHashInitEntry
    };

    if (!PL_DHashTableInit(&sEventListenerManagersHash, &hash_table_ops,
                           nsnull, sizeof(EventListenerManagerMapEntry), 16)) {
      sEventListenerManagersHash.ops = nsnull;

      PL_DHashTableFinish(&sRangeListsHash);
      sRangeListsHash.ops = nsnull;

      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  return NS_OK;
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
  nsIContent *parent = GetParent();
  if (parent) {
    return CallQueryInterface(parent, aParentNode);
  }

  nsIDocument* doc = GetCurrentDoc();
  if (doc) {
    // If we don't have a parent, but we're in the document, we must
    // be the root node of the document. The DOM says that the root
    // is the document.

    return CallQueryInterface(doc, aParentNode);
  }

  *aParentNode = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetPreviousSibling(nsIDOMNode** aPrevSibling)
{
  *aPrevSibling = nsnull;

  nsIContent *sibling = nsnull;
  nsresult rv = NS_OK;

  nsIContent *parent = GetParent();
  if (parent) {
    PRInt32 pos = parent->IndexOf(this);
    if (pos > 0 ) {
      sibling = parent->GetChildAt(pos - 1);
    }
  } else {
    nsIDocument* document = GetCurrentDoc();
    if (document) {
      // Nodes that are just below the document (their parent is the
      // document) need to go to the document to find their next sibling.
      PRInt32 pos = document->IndexOf(this);
      if (pos > 0 ) {
        sibling = document->GetChildAt(pos - 1);
      }
    }
  }

  if (sibling) {
    rv = CallQueryInterface(sibling, aPrevSibling);
    NS_ASSERTION(*aPrevSibling, "Must be a DOM Node");
  }

  return rv;
}

NS_IMETHODIMP
nsGenericElement::GetNextSibling(nsIDOMNode** aNextSibling)
{
  *aNextSibling = nsnull;

  nsIContent *sibling = nsnull;
  nsresult rv = NS_OK;

  nsIContent *parent = GetParent();
  if (parent) {
    PRInt32 pos = parent->IndexOf(this);
    if (pos > -1 ) {
      sibling = parent->GetChildAt(pos + 1);
    }
  } else {
    nsIDocument* document = GetCurrentDoc();
    if (document) {
      // Nodes that are just below the document (their parent is the
      // document) need to go to the document to find their next sibling.
      PRInt32 pos = document->IndexOf(this);
      if (pos > -1 ) {
        sibling = document->GetChildAt(pos + 1);
      }
    }
  }

  if (sibling) {
    rv = CallQueryInterface(sibling, aNextSibling);
    NS_ASSERTION(*aNextSibling, "Must be a DOM Node");
  }

  return rv;
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

  nsCOMPtr<nsINodeInfo> newNodeInfo;
  nsresult rv = nsContentUtils::PrefixChanged(mNodeInfo, prefix,
                                              getter_AddRefs(newNodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  mNodeInfo = newNodeInfo;

  return NS_OK;
}

extern PRBool gCheckedForXPathDOM;
extern PRBool gHaveXPathDOM;

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
  } else if ((!gCheckedForXPathDOM || gHaveXPathDOM) &&
             PL_strcasecmp(f, "XPath") == 0 &&
             (aVersion.IsEmpty() ||
              PL_strcmp(v, "3.0") == 0)) {
    if (!gCheckedForXPathDOM) {
      nsCOMPtr<nsIDOMXPathEvaluator> evaluator =
        do_CreateInstance(NS_XPATH_EVALUATOR_CONTRACTID);
      gHaveXPathDOM = (evaluator != nsnull);
      gCheckedForXPathDOM = PR_TRUE;
    }

    *aReturn = gHaveXPathDOM;
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
  nsDOMSlots *slots = GetDOMSlots();

  if (!slots) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!slots->mChildNodes) {
    slots->mChildNodes = new nsChildContentList(this);
    if (!slots->mChildNodes) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
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

  nsContentList *list = NS_GetContentList(GetCurrentDoc(), nameAtom,
                                          kNameSpaceID_Unknown, this).get();
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
  PRInt32 nameSpaceId = kNameSpaceID_Unknown;

  nsContentList *list = nsnull;

  nsIDocument* document = GetCurrentDoc();
  if (!aNamespaceURI.EqualsLiteral("*")) {
    nameSpaceId =
      nsContentUtils::NameSpaceManager()->GetNameSpaceID(aNamespaceURI);

    if (nameSpaceId == kNameSpaceID_Unknown) {
      // Unknown namespace means no matches, we create an empty list...
      list = NS_GetContentList(document, nsnull,
                               kNameSpaceID_None, nsnull).get();
      NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);
    }
  }

  if (!list) {
    nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(aLocalName);
    NS_ENSURE_TRUE(nameAtom, NS_ERROR_OUT_OF_MEMORY);

    list = NS_GetContentList(document, nameAtom, nameSpaceId, this).get();
    NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);
  }

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
  // XXXbz XUL elements are confused about their current doc when they're
  // cloned, so we don't assert if aParent is a XUL element and aDocument is
  // null, even if aParent->GetCurrentDoc() is non-null
  //  NS_PRECONDITION(!aParent || aDocument == aParent->GetCurrentDoc(),
  //                  "aDocument must be current doc of aParent");
  NS_PRECONDITION(!aParent ||
                  (aParent->IsContentOfType(eXUL) && aDocument == nsnull) ||
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

  // Now set the parent; make sure to preserve the bits we have stashed there
  // Note that checking whether aParent == GetParent() is probably not worth it
  // here.
  PtrBits new_bits = NS_REINTERPRET_CAST(PtrBits, aParent);
  new_bits |= mParentPtrBits & nsIContent::kParentBitMask;
  mParentPtrBits = new_bits;

  nsresult rv;
  
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

    // check the document on the nodeinfo to see whether we need a
    // new nodeinfo
    // XXXbz sXBL/XBL2 issue!
    nsIDocument *ownerDocument = GetOwnerDoc();
    if (aDocument != ownerDocument) {

      if (HasProperties()) {
        ownerDocument->PropertyTable()->DeleteAllPropertiesFor(this);
      }

      // get a new nodeinfo
      nsNodeInfoManager* nodeInfoManager = aDocument->NodeInfoManager();
      if (nodeInfoManager) {
        nsCOMPtr<nsINodeInfo> newNodeInfo;
        rv = nodeInfoManager->GetNodeInfo(mNodeInfo->NameAtom(),
                                          mNodeInfo->GetPrefixAtom(),
                                          mNodeInfo->NamespaceID(),
                                          getter_AddRefs(newNodeInfo));
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ASSERTION(newNodeInfo, "GetNodeInfo lies");
        mNodeInfo.swap(newNodeInfo);
      }
    }
  }

  // Now recurse into our kids
  PRUint32 i, n = GetChildCount();

  for (i = 0; i < n; ++i) {
    rv = mAttrsAndChildren.ChildAt(i)->BindToTree(aDocument, this,
                                                  aBindingParent,
                                                  aCompileEventHandlers);
    NS_ENSURE_SUCCESS(rv, rv);
  }

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

    if (HasAttr(kNameSpaceID_XLink, nsHTMLAtoms::href)) {
      document->ForgetLink(this);
    }

    nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(this);

    if (domElement) {
      nsCOMPtr<nsIDOMNSDocument> nsDoc = do_QueryInterface(document);
      nsDoc->SetBoxObjectFor(domElement, nsnull);
    }
  }

  // Unset things in the reverse order from how we set them in BindToTree
  mParentPtrBits &= ~PARENT_BIT_INDOCUMENT;
  
  if (aNullParent) {
    // Just mask it out
    mParentPtrBits &= nsIContent::kParentBitMask;
  }
  
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
}

PRBool
nsGenericElement::IsNativeAnonymous() const
{
  return !!(GetFlags() & GENERIC_ELEMENT_IS_ANONYMOUS);
}

void
nsGenericElement::SetNativeAnonymous(PRBool aAnonymous)
{
  if (aAnonymous) {
    SetFlags(GENERIC_ELEMENT_IS_ANONYMOUS);
  } else {
    UnsetFlags(GENERIC_ELEMENT_IS_ANONYMOUS);
  }
}

PRInt32
nsGenericElement::GetNameSpaceID() const
{
  return mNodeInfo->NamespaceID();
}

nsIAtom *
nsGenericElement::Tag() const
{
  return mNodeInfo->NameAtom();
}

nsINodeInfo *
nsGenericElement::GetNodeInfo() const
{
  return mNodeInfo;
}

nsresult
nsGenericElement::HandleDOMEvent(nsPresContext* aPresContext,
                                 nsEvent* aEvent,
                                 nsIDOMEvent** aDOMEvent,
                                 PRUint32 aFlags,
                                 nsEventStatus* aEventStatus)
{
  // Make sure to tell the event that dispatch has started.
  NS_MARK_EVENT_DISPATCH_STARTED(aEvent);

  nsresult ret = NS_OK;
  PRBool retarget = PR_FALSE;
  PRBool externalDOMEvent = PR_FALSE;
  nsCOMPtr<nsIDOMEventTarget> oldTarget;

  nsIDOMEvent* domEvent = nsnull;
  if (NS_EVENT_FLAG_INIT & aFlags) {
    if (aDOMEvent) {
      if (*aDOMEvent) {
        externalDOMEvent = PR_TRUE;   
      }
    } else {
      aDOMEvent = &domEvent;
    }
    aEvent->flags |= aFlags;
    aFlags &= ~(NS_EVENT_FLAG_CANT_BUBBLE | NS_EVENT_FLAG_CANT_CANCEL);
    aFlags |= NS_EVENT_FLAG_BUBBLE | NS_EVENT_FLAG_CAPTURE;
  }

  // Find out whether we're anonymous.
  if (IsNativeAnonymous()) {
    retarget = PR_TRUE;
  } else {
    nsIContent* parent = GetParent();
    if (parent) {
      if (*aDOMEvent) {
        (*aDOMEvent)->GetTarget(getter_AddRefs(oldTarget));
        nsCOMPtr<nsIContent> content(do_QueryInterface(oldTarget));
        if (content && content->GetBindingParent() == parent)
          retarget = PR_TRUE;
      } else if (GetBindingParent() == parent) {
        retarget = PR_TRUE;
      }
    }
  }

  // check for an anonymous parent
  nsCOMPtr<nsIContent> parent;
  nsIDocument* ownerDoc = GetOwnerDoc();
  if (ownerDoc) {
    ownerDoc->BindingManager()->GetInsertionParent(this,
                                                   getter_AddRefs(parent));
  }
  if (!parent) {
    // if we didn't find an anonymous parent, use the explicit one,
    // whether it's null or not...
    parent = GetParent();
  }

  if (retarget || (parent.get() != GetParent())) {
    if (!*aDOMEvent) {
      // We haven't made a DOMEvent yet.  Force making one now.
      nsCOMPtr<nsIEventListenerManager> listenerManager;
      if (NS_FAILED(ret = GetListenerManager(getter_AddRefs(listenerManager)))) {
        return ret;
      }
      if (NS_FAILED(ret = listenerManager->CreateEvent(aPresContext, aEvent,
                                                       EmptyString(), aDOMEvent)))
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

    if (!hasOriginal)
      privateEvent->SetOriginalTarget(oldTarget);

    if (retarget) {
      nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(GetParent());
      privateEvent->SetTarget(target);
    }
  }

  //Capturing stage evaluation
  if (NS_EVENT_FLAG_CAPTURE & aFlags &&
      aEvent->message != NS_PAGE_LOAD &&
      aEvent->message != NS_SCRIPT_LOAD &&
      aEvent->message != NS_IMAGE_LOAD &&
      aEvent->message != NS_IMAGE_ERROR &&
      aEvent->message != NS_SCROLL_EVENT) {
    //Initiate capturing phase.  Special case first call to document
    if (parent) {
      parent->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                             aFlags & NS_EVENT_CAPTURE_MASK,
                             aEventStatus);
    } else {
      nsIDocument* document = GetCurrentDoc();
      if (document) {
        ret = document->HandleDOMEvent(aPresContext, aEvent,
                                       aDOMEvent,
                                       aFlags & NS_EVENT_CAPTURE_MASK,
                                       aEventStatus);
      }
    }
  }

  if (retarget) {
    // The event originated beneath us, and we performed a retargeting.
    // We need to restore the original target of the event.
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(*aDOMEvent);
    if (privateEvent)
      privateEvent->SetTarget(oldTarget);
  }

  // Weak pointer, which is fine since the hash table owns the
  // listener manager
  nsIEventListenerManager *lm = nsnull;

  if (HasEventListenerManager()) {
    EventListenerManagerMapEntry *entry =
      NS_STATIC_CAST(EventListenerManagerMapEntry *,
                     PL_DHashTableOperate(&sEventListenerManagersHash, this,
                                          PL_DHASH_LOOKUP));

    if (PL_DHASH_ENTRY_IS_FREE(entry)) {
      NS_ERROR("Huh, our bit says we have an event listener manager, but "
               "there's nothing in the hash!?!!");

      return NS_ERROR_UNEXPECTED;
    }

    lm = entry->mListenerManager;
  }

  //Local handling stage
  if (lm &&
      !(NS_EVENT_FLAG_CANT_BUBBLE & aEvent->flags &&
        NS_EVENT_FLAG_BUBBLE & aFlags && !(NS_EVENT_FLAG_INIT & aFlags)) &&
      !(aEvent->flags & NS_EVENT_FLAG_NO_CONTENT_DISPATCH)) {
    aEvent->flags |= aFlags;

    nsCOMPtr<nsIDOMEventTarget> curTarg =
      do_QueryInterface(NS_STATIC_CAST(nsIXMLContent *, this));

    lm->HandleEvent(aPresContext, aEvent, aDOMEvent, curTarg, aFlags,
                    aEventStatus);

    aEvent->flags &= ~aFlags;

    // We don't want scroll events to bubble further after it has been
    // handled at the local stage.
    if (aEvent->message == NS_SCROLL_EVENT && aFlags & NS_EVENT_FLAG_BUBBLE)
      aEvent->flags |= NS_EVENT_FLAG_CANT_BUBBLE;
  }

  if (retarget) {
    // The event originated beneath us, and we need to perform a
    // retargeting.
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(*aDOMEvent);
    if (privateEvent) {
      nsCOMPtr<nsIDOMEventTarget> parentTarget(do_QueryInterface(GetParent()));
      privateEvent->SetTarget(parentTarget);
    }
  }

  //Bubbling stage
  if (NS_EVENT_FLAG_BUBBLE & aFlags && IsInDoc() &&
      aEvent->message != NS_PAGE_LOAD && aEvent->message != NS_SCRIPT_LOAD &&
      aEvent->message != NS_IMAGE_ERROR && aEvent->message != NS_IMAGE_LOAD &&
      !(aEvent->message == NS_SCROLL_EVENT &&
        aEvent->flags & NS_EVENT_FLAG_CANT_BUBBLE)) {
    if (parent) {
      // If there's a parent we pass the event to the parent...

      ret = parent->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                   aFlags & NS_EVENT_BUBBLE_MASK,
                                   aEventStatus);
    } else {
      // If there's no parent but there is a document (i.e. this is
      // the root node) we pass the event to the document...
      nsIDocument* document = GetCurrentDoc();
      if (document) {
        ret = document->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                       aFlags & NS_EVENT_BUBBLE_MASK, 
                                       aEventStatus);
      }
    }
  }

  if (retarget) {
    // The event originated beneath us, and we performed a
    // retargeting.  We need to restore the original target of the
    // event.

    nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(*aDOMEvent);
    if (privateEvent)
      privateEvent->SetTarget(oldTarget);
  }

  if (NS_EVENT_FLAG_INIT & aFlags) {
    // We're leaving the DOM event loop so if we created a DOM event,
    // release here.  If externalDOMEvent is set the event was passed
    // in and we don't own it

    if (*aDOMEvent && !externalDOMEvent) {
      nsrefcnt rc;
      NS_RELEASE2(*aDOMEvent, rc);
      if (0 != rc) {
        // Okay, so someone in the DOM loop (a listener, JS object)
        // still has a ref to the DOM Event but the internal data
        // hasn't been malloc'd.  Force a copy of the data here so the
        // DOM Event is still valid.

        nsCOMPtr<nsIPrivateDOMEvent> privateEvent =
          do_QueryInterface(*aDOMEvent);

        if (privateEvent) {
          privateEvent->DuplicatePrivateData();
        }
      }

      aDOMEvent = nsnull;
    }

    // Now that we're done with this event, remove the flag that says
    // we're in the process of dispatching this event.
    NS_MARK_EVENT_DISPATCH_DONE(aEvent);
  }

  return ret;
}

PRUint32
nsGenericElement::ContentID() const
{
  nsDOMSlots *slots = GetExistingDOMSlots();

  if (slots) {
    return slots->mContentID;
  }

  PtrBits flags = GetFlags();

  return flags >> GENERIC_ELEMENT_CONTENT_ID_BITS_OFFSET;
}

void
nsGenericElement::SetContentID(PRUint32 aID)
{
  // This should be in the constructor!!!

  if (HasDOMSlots() || aID > GENERIC_ELEMENT_CONTENT_ID_MAX_VALUE) {
    nsDOMSlots *slots = GetDOMSlots();

    if (slots) {
      slots->mContentID = aID;
    }
  } else {
    UnsetFlags(GENERIC_ELEMENT_CONTENT_ID_MASK);
    SetFlags(aID << GENERIC_ELEMENT_CONTENT_ID_BITS_OFFSET);
  }
}

NS_IMETHODIMP
nsGenericElement::MaybeTriggerAutoLink(nsIDocShell *aShell)
{
  return NS_OK;
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

NS_IMETHODIMP_(PRBool)
nsGenericElement::HasClass(nsIAtom* aClass, PRBool aCaseSensitive) const
{
  return PR_FALSE;
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
  nsresult rv = GetAttr(kNameSpaceID_XML, nsHTMLAtoms::base, value);
  if (rv != NS_CONTENT_ATTR_HAS_VALUE) {
    // No xml:base, so we just use the parent's base URL
    nsIURI *base = parentBase;
    NS_IF_ADDREF(base);

    return base;
  }

  nsCOMPtr<nsIURI> ourBase;
  rv = NS_NewURI(getter_AddRefs(ourBase), value,
                 doc->GetDocumentCharacterSet().get(), parentBase);
  if (NS_SUCCEEDED(rv)) {
    // do a security check, almost the same as nsDocument::SetBaseURL()
    rv = nsContentUtils::SecurityManager()->
      CheckLoadURIWithPrincipal(doc->GetPrincipal(), ourBase,
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

nsresult
nsGenericElement::RangeAdd(nsIDOMRange* aRange)
{
  if (!sRangeListsHash.ops) {
    // We've already been shut down, don't bother adding a range...

    return NS_OK;
  }

  RangeListMapEntry *entry =
    NS_STATIC_CAST(RangeListMapEntry *,
                   PL_DHashTableOperate(&sRangeListsHash, this, PL_DHASH_ADD));

  if (!entry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // lazy allocation of range list
  if (!entry->mRangeList) {
    NS_ASSERTION(!(GetFlags() & GENERIC_ELEMENT_HAS_RANGELIST),
                 "Huh, nsGenericElement flags don't reflect reality!!!");

    entry->mRangeList = new nsAutoVoidArray();

    if (!entry->mRangeList) {
      PL_DHashTableRawRemove(&sRangeListsHash, entry);

      return NS_ERROR_OUT_OF_MEMORY;
    }

    SetFlags(GENERIC_ELEMENT_HAS_RANGELIST);
  }

  // Make sure we don't add a range that is already in the list!
  PRInt32 i = entry->mRangeList->IndexOf(aRange);

  if (i >= 0) {
    // Range is already in the list, so there is nothing to do!

    return NS_OK;
  }

  // dont need to addref - this call is made by the range object
  // itself
  PRBool rv = entry->mRangeList->AppendElement(aRange);
  if (!rv) {
    if (entry->mRangeList->Count() == 0) {
      // Fresh entry, remove it from the hash...

      PL_DHashTableRawRemove(&sRangeListsHash, entry);
    }

    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}


void
nsGenericElement::RangeRemove(nsIDOMRange* aRange)
{
  if (!HasRangeList()) {
    return;
  }

  RangeListMapEntry *entry =
    NS_STATIC_CAST(RangeListMapEntry *,
                   PL_DHashTableOperate(&sRangeListsHash, this,
                                        PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_FREE(entry)) {
    NS_ERROR("Huh, our bit says we have a range list, but there's nothing "
             "in the hash!?!!");

    return;
  }

  if (!entry->mRangeList) {
    return;
  }

  // dont need to release - this call is made by the range object itself
  entry->mRangeList->RemoveElement(aRange);

  if (entry->mRangeList->Count() == 0) {
    PL_DHashTableRawRemove(&sRangeListsHash, entry);

    UnsetFlags(GENERIC_ELEMENT_HAS_RANGELIST);
  }
}

const nsVoidArray *
nsGenericElement::GetRangeList() const
{
  if (!HasRangeList()) {
    return nsnull;
  }

  RangeListMapEntry *entry =
    NS_STATIC_CAST(RangeListMapEntry *,
                   PL_DHashTableOperate(&sRangeListsHash, this,
                                        PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_FREE(entry)) {
    NS_ERROR("Huh, our bit says we have a range list, but there's nothing "
             "in the hash!?!!");

    return nsnull;
  }

  return entry->mRangeList;
}

void
nsGenericElement::SetFocus(nsPresContext* aPresContext)
{
  // Traditionally focusable elements can take focus as long as they don't set
  // the disabled attribute

  nsIPresShell *presShell = aPresContext->PresShell();
  nsIFrame* frame = presShell->GetPrimaryFrameFor(this);
  if (frame && frame->IsFocusable() &&
    aPresContext->EventStateManager()->SetContentState(this,
                                                       NS_EVENT_STATE_FOCUS)) {
    // Reget frame, setting content state can change style which can
    // cause a frame reconstruction, for example if CSS overflow changes
    frame = presShell->GetPrimaryFrameFor(this);
    if (frame) {
      presShell->ScrollFrameIntoView(frame, NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,
                                    NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE);
    }
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
nsGenericElement::IsContentOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~eELEMENT);
}

//----------------------------------------------------------------------

nsresult
nsGenericElement::GetListenerManager(nsIEventListenerManager **aResult)
{
  *aResult = nsnull;

  if (!sEventListenerManagersHash.ops) {
    // We're already shut down, don't bother creating a event listener
    // manager.

    return NS_ERROR_NOT_AVAILABLE;
  }

  EventListenerManagerMapEntry *entry =
    NS_STATIC_CAST(EventListenerManagerMapEntry *,
                   PL_DHashTableOperate(&sEventListenerManagersHash, this,
                                        PL_DHASH_ADD));

  if (!entry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!entry->mListenerManager) {
    nsresult rv =
      NS_NewEventListenerManager(getter_AddRefs(entry->mListenerManager));

    if (NS_FAILED(rv)) {
      PL_DHashTableRawRemove(&sEventListenerManagersHash, entry);

      return rv;
    }

    entry->mListenerManager->SetListenerTarget(NS_STATIC_CAST(nsIXMLContent *,
                                                              this));

    SetFlags(GENERIC_ELEMENT_HAS_LISTENERMANAGER);
  }

  *aResult = entry->mListenerManager;
  NS_ADDREF(*aResult);

  return NS_OK;
}

// virtual
void
nsGenericElement::SetMayHaveFrame(PRBool aMayHaveFrame)
{
  if (aMayHaveFrame) {
    SetFlags(GENERIC_ELEMENT_MAY_HAVE_FRAME);
  } else {
    UnsetFlags(GENERIC_ELEMENT_MAY_HAVE_FRAME);
  }
}

// virtual
PRBool
nsGenericElement::MayHaveFrame() const
{
  return !!(GetFlags() & GENERIC_ELEMENT_MAY_HAVE_FRAME);
}

nsresult
nsGenericElement::InsertChildAt(nsIContent* aKid,
                                PRUint32 aIndex,
                                PRBool aNotify)
{
  NS_PRECONDITION(aKid, "null ptr");

  nsIDocument *document = GetCurrentDoc();
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, aNotify);
  
  nsresult rv = mAttrsAndChildren.InsertChildAt(aKid, aIndex);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aKid->BindToTree(document, this, nsnull, PR_TRUE);
  if (NS_FAILED(rv)) {
    mAttrsAndChildren.RemoveChildAt(aIndex);
    aKid->UnbindFromTree();
    return rv;
  }
  
  nsRange::OwnerChildInserted(this, aIndex);
  // The kid may have removed us from the document, so recheck that we're still
  // in the document before proceeding.  Also, the kid may have just removed
  // itself, in which case we don't really want to fire ContentAppended or a
  // mutation event.
  // XXXbz What if the kid just moved us in the document?  Scripts suck.  We
  // really need to stop running them while we're in the middle of modifying
  // the DOM....
  if (document && document == GetCurrentDoc() && aKid->GetParent() == this) {
    if (aNotify) {
      if (aIndex == GetChildCount() - 1) {
        document->ContentAppended(this, aIndex);
      } else {
        document->ContentInserted(this, aKid, aIndex);
      }
    }

    if (HasMutationListeners(this, NS_EVENT_BITS_MUTATION_NODEINSERTED)) {
      nsMutationEvent mutation(PR_TRUE, NS_MUTATION_NODEINSERTED, aKid);
      mutation.mRelatedNode = do_QueryInterface(this);

      nsEventStatus status = nsEventStatus_eIgnore;
      aKid->HandleDOMEvent(nsnull, &mutation, nsnull, NS_EVENT_FLAG_INIT, &status);
    }
  }

  return NS_OK;
}

nsresult
nsGenericElement::AppendChildTo(nsIContent* aKid, PRBool aNotify)
{
  NS_PRECONDITION(aKid && this != aKid, "null ptr");
  nsIDocument *document = GetCurrentDoc();
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, aNotify);
  
  nsresult rv = mAttrsAndChildren.AppendChild(aKid);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = aKid->BindToTree(document, this, nsnull, PR_TRUE);
  if (NS_FAILED(rv)) {
    mAttrsAndChildren.RemoveChildAt(GetChildCount() - 1);
    aKid->UnbindFromTree();
    return rv;
  }
  // ranges don't need adjustment since new child is at end of list

  // The kid may have removed us from the document, so recheck that we're still
  // in the document before proceeding.  Also, the kid may have just removed
  // itself, in which case we don't really want to fire ContentAppended or a
  // mutation event.
  // XXXbz What if the kid just moved us in the document?  Scripts suck.  We
  // really need to stop running them while we're in the middle of modifying
  // the DOM....
  if (document && document == GetCurrentDoc() && aKid->GetParent() == this) {
    if (aNotify) {
      document->ContentAppended(this, GetChildCount() - 1);
    }

    if (HasMutationListeners(this, NS_EVENT_BITS_MUTATION_NODEINSERTED)) {
      nsMutationEvent mutation(PR_TRUE, NS_MUTATION_NODEINSERTED, aKid);
      mutation.mRelatedNode = do_QueryInterface(this);

      nsEventStatus status = nsEventStatus_eIgnore;
      aKid->HandleDOMEvent(nsnull, &mutation, nsnull, NS_EVENT_FLAG_INIT, &status);
    }
  }

  return NS_OK;
}

nsresult
nsGenericElement::RemoveChildAt(PRUint32 aIndex, PRBool aNotify)
{
  nsCOMPtr<nsIContent> oldKid = GetChildAt(aIndex);
  if (oldKid) {
    nsIDocument *document = GetCurrentDoc();
    mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, aNotify);

    PRBool hasListeners = 
      HasMutationListeners(this, NS_EVENT_BITS_MUTATION_NODEREMOVED);

    if (hasListeners) {
      nsMutationEvent mutation(PR_TRUE, NS_MUTATION_NODEREMOVED, oldKid);
      mutation.mRelatedNode = do_QueryInterface(this);

      nsEventStatus status = nsEventStatus_eIgnore;
      oldKid->HandleDOMEvent(nsnull, &mutation, nsnull,
                             NS_EVENT_FLAG_INIT, &status);
    }

    // Someone may have removed the kid while that event was processing...
    if (!hasListeners ||
        (oldKid->GetParent() == this && oldKid == GetChildAt(aIndex))) {
      nsRange::OwnerChildRemoved(this, aIndex, oldKid);

      mAttrsAndChildren.RemoveChildAt(aIndex);

      if (aNotify && document) {
        document->ContentRemoved(this, oldKid, aIndex);
      }
    
      oldKid->UnbindFromTree();
    }
  }

  return NS_OK;
}



//----------------------------------------------------------------------

// Generic DOMNode implementations

/*
 * This helper function checks if aChild is the same as aNode or if
 * aChild is one of aNode's ancestors. -- jst@citec.fi
 */

/* static */
PRBool
nsGenericElement::isSelfOrAncestor(nsIContent *aNode,
                                   nsIContent *aPossibleAncestor)
{
  NS_PRECONDITION(aNode, "Must have a node");
  
  if (aNode == aPossibleAncestor)
    return PR_TRUE;

  /*
   * If aPossibleAncestor doesn't have children it can't be our ancestor
   */
  if (aPossibleAncestor->GetChildCount() == 0) {
    return PR_FALSE;
  }

  for (nsIContent* ancestor = aNode->GetParent();
       ancestor;
       ancestor = ancestor->GetParent()) {
    if (ancestor == aPossibleAncestor) {
      /*
       * We found aPossibleAncestor as one of our ancestors
       */
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

NS_IMETHODIMP
nsGenericElement::InsertBefore(nsIDOMNode *aNewChild, nsIDOMNode *aRefChild,
                               nsIDOMNode **aReturn)
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

    refPos = IndexOf(refContent);

    if (refPos < 0) {
      return NS_ERROR_DOM_NOT_FOUND_ERR;
    }
  } else {
    refPos = GetChildCount();
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

  nsCOMPtr<nsIDocument> old_doc = newContent->GetDocument();
  if (old_doc && old_doc != GetDocument() &&
      !nsContentUtils::CanCallerAccess(aNewChild)) {
    return NS_ERROR_DOM_SECURITY_ERR;
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
   * Check if we're inserting a document fragment. If we are, we need
   * to remove the children of the document fragment and add them
   * individually (i.e. we don't add the actual document fragment).
   */
  if (nodeType == nsIDOMNode::DOCUMENT_FRAGMENT_NODE) {
    nsCOMPtr<nsIDocumentFragment> doc_fragment(do_QueryInterface(newContent));
    NS_ENSURE_TRUE(doc_fragment, NS_ERROR_UNEXPECTED);

    doc_fragment->DisconnectChildren();

    PRUint32 count = newContent->GetChildCount();

    PRUint32 old_count = GetChildCount();

    PRBool do_notify = !!aRefChild;

    // If do_notify is true, then we don't have to handle the notifications
    // ourselves...  Also, if count is 0 there will be no updates.  So we only
    // want an update batch to happen if count is nonzero and do_notify is not
    // true.
    mozAutoDocUpdate updateBatch(GetDocument(), UPDATE_CONTENT_MODEL,
                                 count && !do_notify);
    
    /*
     * Iterate through the fragments children, removing each from
     * the fragment and inserting it into the child list of its
     * new parent.
     */

    nsCOMPtr<nsIContent> childContent;

    for (PRUint32 i = 0; i < count; ++i) {
      // Get the n:th child from the document fragment. Since we
      // disconnected the children from the document fragment they
      // won't be removed from the document fragment when inserted
      // into the new parent. This lets us do this operation *much*
      // faster.
      childContent = newContent->GetChildAt(i);

      // Insert the child and increment the insertion position
      res = InsertChildAt(childContent, refPos++, do_notify);

      if (NS_FAILED(res)) {
        break;
      }
    }

    if (NS_FAILED(res)) {
      // This should put the children that were moved out of the
      // document fragment back into the document fragment and remove
      // them from the element they were intserted into.

      doc_fragment->ReconnectChildren();

      return res;
    }

    nsIDocument *doc = GetDocument();

    if (count && !do_notify && doc) {
      doc->ContentAppended(this, old_count);
    }

    doc_fragment->DropChildReferences();
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

      PRUint32 origChildCount = GetChildCount();

      /*
       * We don't care here if the return fails or not.
       */
      oldParent->RemoveChild(aNewChild, getter_AddRefs(tmpNode));

      PRUint32 newChildCount = GetChildCount();

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

          if (refContent == GetChildAt(refPos - 1)) {
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

    if (!newContent->IsContentOfType(eXUL)) {
      nsContentUtils::ReparentContentWrapper(newContent, this, GetDocument(),
                                             old_doc);
    }

    res = InsertChildAt(newContent, refPos, PR_TRUE);

    if (NS_FAILED(res)) {
      return res;
    }
  }

  *aReturn = aNewChild;
  NS_ADDREF(*aReturn);

  return res;
}

NS_IMETHODIMP
nsGenericElement::ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
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

  nsCOMPtr<nsIContent> oldContent = do_QueryInterface(aOldChild);

  // if oldContent is null IndexOf will return < 0, which is what we want
  // since aOldChild couldn't be a child.
  oldPos = IndexOf(oldContent);
  if (oldPos < 0) {
    return NS_ERROR_DOM_NOT_FOUND_ERR;
  }

  nsCOMPtr<nsIContent> replacedChild = GetChildAt(oldPos);

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

  nsIDocument* elemDoc = GetDocument();

  nsCOMPtr<nsIDocument> old_doc = newContent->GetDocument();
  if (old_doc && old_doc != elemDoc &&
      !nsContentUtils::CanCallerAccess(aNewChild)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  /*
   * Make sure the new child is not "this" node or one of this nodes
   * ancestors. Doing this check here should be safe even if newContent
   * is a document fragment.
   */
  if (isSelfOrAncestor(this, newContent)) {
    return NS_ERROR_DOM_HIERARCHY_REQUEST_ERR;
  }

  // We're ready to start inserting children, so let's start a batch
  mozAutoDocUpdate updateBatch(elemDoc, UPDATE_CONTENT_MODEL, PR_TRUE);

  /*
   * Check if this is a document fragment. If it is, we need
   * to remove the children of the document fragment and add them
   * individually (i.e. we don't add the actual document fragment).
   */
  if (nodeType == nsIDOMNode::DOCUMENT_FRAGMENT_NODE) {
    nsCOMPtr<nsIContent> childContent;
    PRUint32 i, count = newContent->GetChildCount();
    res = RemoveChildAt(oldPos, PR_TRUE);
    NS_ENSURE_SUCCESS(res, res);

    /*
     * Iterate through the fragments children, removing each from
     * the fragment and inserting it into the child list of its
     * new parent.
     */
    for (i = 0; i < count; ++i) {
      // Always get and remove the first child, since the child indexes
      // change as we go along.
      childContent = newContent->GetChildAt(0);
      res = newContent->RemoveChildAt(0, PR_FALSE);
      NS_ENSURE_SUCCESS(res, res);

      // Insert the child and increment the insertion position
      res = InsertChildAt(childContent, oldPos++, PR_TRUE);
      NS_ENSURE_SUCCESS(res, res);
    }
  }
  else {
    nsCOMPtr<nsIDOMNode> oldParent;
    res = aNewChild->GetParentNode(getter_AddRefs(oldParent));
    NS_ENSURE_SUCCESS(res, res);

    /*
     * Remove the element from the old parent if one exists, since oldParent
     * is a nsIDOMNode this will do the right thing even if the parent of
     * aNewChild is a document. This code also handles the case where the
     * new child is alleady a child of this node-- jst@citec.fi
     */
    if (oldParent) {
      PRUint32 origChildCount = GetChildCount();

      /*
       * We don't care here if the return fails or not.
       */
      nsCOMPtr<nsIDOMNode> tmpNode;
      oldParent->RemoveChild(aNewChild, getter_AddRefs(tmpNode));

      PRUint32 newChildCount = GetChildCount();

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
        nsIContent *tmpContent = GetChildAt(oldPos - 1);

        if (oldContent == tmpContent) {
          oldPos--;
        }
      }
    }

    if (!newContent->IsContentOfType(eXUL)) {
      nsContentUtils::ReparentContentWrapper(newContent, this, GetDocument(),
                                             old_doc);
    }

    // If we're replacing a child with itself the child
    // has already been removed from this element once we get here.
    if (aNewChild != aOldChild) {
      res = RemoveChildAt(oldPos, PR_TRUE);
      NS_ENSURE_SUCCESS(res, res);
    }

    res = InsertChildAt(newContent, oldPos, PR_TRUE);
    NS_ENSURE_SUCCESS(res, res);
  }

  return CallQueryInterface(replacedChild, aReturn);
}

NS_IMETHODIMP
nsGenericElement::RemoveChild(nsIDOMNode *aOldChild, nsIDOMNode **aReturn)
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

  PRInt32 pos = IndexOf(content);

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

NS_INTERFACE_MAP_BEGIN(nsGenericElement)
  NS_INTERFACE_MAP_ENTRY(nsIContent)
  NS_INTERFACE_MAP_ENTRY(nsIStyledContent)
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOM3Node, new nsNode3Tearoff(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMEventReceiver,
                                 nsDOMEventRTTearoff::Create(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMEventTarget,
                                 nsDOMEventRTTearoff::Create(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOM3EventTarget,
                                 nsDOMEventRTTearoff::Create(this))
  NS_INTERFACE_MAP_ENTRY_TEAROFF(nsIDOMNSEventTarget,
                                 nsDOMEventRTTearoff::Create(this))
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContent)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsGenericElement)
NS_IMPL_RELEASE(nsGenericElement)

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
                              nsLinkVerb aVerb,
                              nsIURI* aOriginURI,
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
      PRUint32 flag = aIsUserTriggered ?
                      (PRUint32) nsIScriptSecurityManager::STANDARD :
                      (PRUint32) nsIScriptSecurityManager::DISALLOW_FROM_MAIL;
      proceed =
        securityManager->CheckLoadURI(aOriginURI, aLinkURI, flag);
    }

    // Only pass off the click event if the script security manager
    // says it's ok.
    if (NS_SUCCEEDED(proceed))
      handler->OnLinkClick(this, aVerb, aLinkURI, aTargetSpec.get());
  } else {
    handler->OnOverLink(this, aLinkURI, aTargetSpec.get());
  }
  return rv;
}

nsresult
nsGenericElement::AddScriptEventListener(nsIAtom* aAttribute,
                                         const nsAString& aValue)
{
  nsresult rv = NS_OK;
  nsISupports *target = NS_STATIC_CAST(nsIContent *, this);
  PRBool defer = PR_TRUE;

  nsCOMPtr<nsIEventListenerManager> manager;

  // Attributes on the body and frameset tags get set on the global object
  if (mNodeInfo->Equals(nsHTMLAtoms::body) ||
      mNodeInfo->Equals(nsHTMLAtoms::frameset)) {
    nsIScriptGlobalObject *sgo;

    // If we have a document, and it has a script global, add the
    // event listener on the global. If not, proceed as normal.
    // XXXbz should we instead use GetCurrentDoc() here, override
    // BindToTree for those classes and munge event listeners there?
    nsIDocument *document = GetOwnerDoc();
    if (document && (sgo = document->GetScriptGlobalObject())) {
      nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(sgo));
      NS_ENSURE_TRUE(receiver, NS_ERROR_FAILURE);

      receiver->GetListenerManager(getter_AddRefs(manager));

      target = sgo;
      defer = PR_FALSE;
    }
  } else {
    GetListenerManager(getter_AddRefs(manager));
  }

  if (manager) {
    nsIDocument *ownerDoc = GetOwnerDoc();

    rv =
      manager->AddScriptEventListener(target, aAttribute, aValue, defer,
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
nsGenericElement::CopyInnerTo(nsGenericElement* aDst, PRBool aDeep) const
{
  nsresult rv = NS_OK;
  PRUint32 i, count = mAttrsAndChildren.AttrCount();
  for (i = 0; i < count; ++i) {
    // XXX Once we have access to existing nsDOMAttributes for this element, we
    //     should call CloneNode or ImportNode on them.
    const nsAttrName* name = mAttrsAndChildren.GetSafeAttrNameAt(i);
    const nsAttrValue* value = mAttrsAndChildren.AttrAt(i);
    nsAutoString valStr;
    value->ToString(valStr);
    rv = aDst->SetAttr(name->NamespaceID(), name->LocalName(),
                       name->GetPrefix(), valStr, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (aDeep) {
    nsIDocument *doc = GetOwnerDoc();
    nsIDocument *newDoc = aDst->GetOwnerDoc();
    if (doc == newDoc) {
      rv = CloneChildrenTo(aDst);
    }
    else {
      nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(newDoc);
      rv = ImportChildrenTo(aDst, domDoc);
    }
  }

  return rv;
}

nsresult
nsGenericElement::ImportChildrenTo(nsGenericElement *aDst,
                                   nsIDOMDocument *aImportDocument) const
{
  PRUint32 i, count = mAttrsAndChildren.ChildCount();
  for (i = 0; i < count; ++i) {
    nsresult rv;
    nsCOMPtr<nsIDOMNode> node =
      do_QueryInterface(mAttrsAndChildren.ChildAt(i), &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> newNode;
    rv = aImportDocument->ImportNode(node, PR_TRUE, getter_AddRefs(newNode));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIContent> newContent = do_QueryInterface(newNode, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aDst->AppendChildTo(newContent, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
nsGenericElement::CloneChildrenTo(nsGenericElement *aDst) const
{
  PRUint32 i, count = mAttrsAndChildren.ChildCount();
  for (i = 0; i < count; ++i) {
    nsresult rv;
    nsCOMPtr<nsIDOMNode> node =
      do_QueryInterface(mAttrsAndChildren.ChildAt(i), &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMNode> newNode;
    rv = node->CloneNode(PR_TRUE, getter_AddRefs(newNode));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIContent> newContent = do_QueryInterface(newNode, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aDst->AppendChildTo(newContent, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

static PRBool
NodeHasMutationListeners(nsISupports* aNode)
{
  nsCOMPtr<nsIDOMEventReceiver> rec(do_QueryInterface(aNode));
  if (rec) {
    nsCOMPtr<nsIEventListenerManager> manager;
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

// Static helper method

PRBool
nsGenericElement::HasMutationListeners(nsIContent* aContent, PRUint32 aType)
{
  nsIDocument* doc = aContent->GetDocument();
  if (!doc)
    return PR_FALSE;

  nsIScriptGlobalObject *global = doc->GetScriptGlobalObject();
  if (!global)
    return PR_FALSE;

  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(global));
  if (!window)
    return PR_FALSE;

  if (!window->HasMutationListeners(aType))
    return PR_FALSE;

  // We know a mutation listener is registered, but it might not
  // be in our chain.  Check quickly to see.

  for (nsIContent* curr = aContent; curr; curr = curr->GetParent())
    if (NodeHasMutationListeners(curr))
      return PR_TRUE;

  return NodeHasMutationListeners(doc) || NodeHasMutationListeners(window);
}

nsresult
nsGenericElement::SetAttr(PRInt32 aNamespaceID, nsIAtom* aName,
                          nsIAtom* aPrefix, const nsAString& aValue,
                          PRBool aNotify)
{
  NS_ENSURE_ARG_POINTER(aName);
  NS_ASSERTION(aNamespaceID != kNameSpaceID_Unknown,
               "Don't call SetAttr with unknown namespace");

  if (kNameSpaceID_XLink == aNamespaceID && nsHTMLAtoms::href == aName) {
    // XLink URI(s) might be changing. Drop the link from the map. If it
    // is still style relevant it will be re-added by
    // nsStyleUtil::IsSimpleXlink. Make sure to keep the style system
    // consistent so this remains true! In particular if the style system
    // were to get smarter and not restyling an XLink element if the href
    // doesn't change in a "significant" way, we'd need to do the same
    // significance check here.
    nsIDocument* doc = GetCurrentDoc();
    if (doc) {
      doc->ForgetLink(this);
    }
  }

  PRBool modification = PR_FALSE;
  nsAutoString oldValue;

  PRInt32 index = mAttrsAndChildren.IndexOfAttr(aName, aNamespaceID);
  if (index >= 0) {
    modification = PR_TRUE;
    
    // Get old value and see if it's the same as new one
    const nsAttrName* attrName = mAttrsAndChildren.GetSafeAttrNameAt(index);
    const nsAttrValue* val = mAttrsAndChildren.AttrAt(index);
    NS_ASSERTION(attrName && val, "attribute is supposed to be there");
    val->ToString(oldValue);
    if (oldValue.Equals(aValue) &&
        aPrefix == attrName->GetPrefix()) {
      // Nothing to do

      return NS_OK;
    }
  }

  // Begin the update _before_ changing the attr value
  nsIDocument *document = GetCurrentDoc();
  mozAutoDocUpdate updateBatch(document, UPDATE_CONTENT_MODEL, aNotify);
  
  if (aNotify && document) {
    document->AttributeWillChange(this, aNamespaceID, aName);
  }

  nsresult rv;
  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == GetIDAttributeName() && !aValue.IsEmpty()) {
      // Store id as atom. id="" means that the element has no id, not that it has
      // an emptystring as the id.
      nsAttrValue attrValue;
      attrValue.ParseAtom(aValue);
      rv = mAttrsAndChildren.SetAndTakeAttr(aName, attrValue);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      rv = mAttrsAndChildren.SetAttr(aName, aValue);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  else {
    nsCOMPtr<nsINodeInfo> ni;
    rv = mNodeInfo->NodeInfoManager()->GetNodeInfo(aName, aPrefix,
                                                   aNamespaceID,
                                                   getter_AddRefs(ni));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAttrValue attrVal(aValue);
    rv = mAttrsAndChildren.SetAndTakeAttr(ni, attrVal);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (document) {
    nsXBLBinding *binding = document->BindingManager()->GetBinding(this);
    if (binding)
      binding->AttributeChanged(aName, aNamespaceID, PR_FALSE, aNotify);

    if (HasMutationListeners(this, NS_EVENT_BITS_MUTATION_ATTRMODIFIED)) {
      nsCOMPtr<nsIDOMEventTarget> node =
        do_QueryInterface(NS_STATIC_CAST(nsIContent *, this));
      nsMutationEvent mutation(PR_TRUE, NS_MUTATION_ATTRMODIFIED, node);

      nsAutoString attrName;
      aName->ToString(attrName);
      nsCOMPtr<nsIDOMAttr> attrNode;
      GetAttributeNode(attrName, getter_AddRefs(attrNode));
      mutation.mRelatedNode = attrNode;

      mutation.mAttrName = aName;
      if (!oldValue.IsEmpty()) {
        mutation.mPrevAttrValue = do_GetAtom(oldValue);
      }

      if (!aValue.IsEmpty()) {
        mutation.mNewAttrValue = do_GetAtom(aValue);
      }

      if (modification) {
        mutation.mAttrChange = nsIDOMMutationEvent::MODIFICATION;
      } else {
        mutation.mAttrChange = nsIDOMMutationEvent::ADDITION;
      }

      nsEventStatus status = nsEventStatus_eIgnore;
      HandleDOMEvent(nsnull, &mutation, nsnull,
                     NS_EVENT_FLAG_INIT, &status);
    }

    if (aNotify) {
      PRInt32 modHint = modification ? PRInt32(nsIDOMMutationEvent::MODIFICATION)
                                     : PRInt32(nsIDOMMutationEvent::ADDITION);
      document->AttributeChanged(this, aNamespaceID, aName, modHint);
    }
  }

  if (aNamespaceID == kNameSpaceID_XMLEvents && aName == nsHTMLAtoms::_event && 
      mNodeInfo->GetDocument()) {
    mNodeInfo->GetDocument()->AddXMLEventsContent(this);
  }

  return NS_OK;
}

nsresult
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
    
    return NS_CONTENT_ATTR_NOT_THERE;
  }

  val->ToString(aResult);

  return aResult.IsEmpty() ? NS_CONTENT_ATTR_NO_VALUE :
                             NS_CONTENT_ATTR_HAS_VALUE;
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
    if (kNameSpaceID_XLink == aNameSpaceID && nsHTMLAtoms::href == aName) {
      // XLink URI might be changing.
      document->ForgetLink(this);
    }

    if (aNotify) {
      document->AttributeWillChange(this, aNameSpaceID, aName);
    }

    if (HasMutationListeners(this, NS_EVENT_BITS_MUTATION_ATTRMODIFIED)) {
      nsCOMPtr<nsIDOMEventTarget> node =
        do_QueryInterface(NS_STATIC_CAST(nsIContent *, this));
      nsMutationEvent mutation(PR_TRUE, NS_MUTATION_ATTRMODIFIED, node);

      nsAutoString attrName;
      aName->ToString(attrName);
      nsCOMPtr<nsIDOMAttr> attrNode;
      GetAttributeNode(attrName, getter_AddRefs(attrNode));
      mutation.mRelatedNode = attrNode;
      mutation.mAttrName = aName;

      nsAutoString value;
      // It sucks that we have to call GetAttr here, but HTML can't always
      // get the value from the nsAttrAndChildArray. Specifically enums and
      // nsISupports can't be converted to strings.
      GetAttr(aNameSpaceID, aName, value);
      if (!value.IsEmpty())
        mutation.mPrevAttrValue = do_GetAtom(value);
      mutation.mAttrChange = nsIDOMMutationEvent::REMOVAL;

      nsEventStatus status = nsEventStatus_eIgnore;
      HandleDOMEvent(nsnull, &mutation, nsnull,
                     NS_EVENT_FLAG_INIT, &status);
    }
  }

  // Clear binding to nsIDOMNamedNodeMap
  nsDOMSlots *slots = GetExistingDOMSlots();
  if (slots && slots->mAttributeMap) {
    slots->mAttributeMap->DropAttribute(aNameSpaceID, aName);
  }

  nsresult rv = mAttrsAndChildren.RemoveAttrAt(index);
  NS_ENSURE_SUCCESS(rv, rv);

  if (document) {
    nsXBLBinding *binding = document->BindingManager()->GetBinding(this);
    if (binding)
      binding->AttributeChanged(aName, aNameSpaceID, PR_TRUE, aNotify);

    if (aNotify) {
      document->AttributeChanged(this, aNameSpaceID, aName,
                                 nsIDOMMutationEvent::REMOVAL);
    }
  }

  return NS_OK;
}

nsresult
nsGenericElement::GetAttrNameAt(PRUint32 aIndex, PRInt32* aNameSpaceID,
                                nsIAtom** aName, nsIAtom** aPrefix) const
{
  const nsAttrName* name = mAttrsAndChildren.GetSafeAttrNameAt(aIndex);
  if (name) {
    *aNameSpaceID = name->NamespaceID();
    NS_ADDREF(*aName = name->LocalName());
    NS_IF_ADDREF(*aPrefix = name->GetPrefix());

    return NS_OK;
  }

  *aNameSpaceID = kNameSpaceID_None;
  *aName = nsnull;
  *aPrefix = nsnull;

  return NS_ERROR_ILLEGAL_VALUE;
}

PRUint32
nsGenericElement::GetAttrCount() const
{
  return mAttrsAndChildren.AttrCount();
}

#ifdef DEBUG
void
nsGenericElement::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(IsInDoc(), "bad content");

  PRInt32 indent;
  for (indent = aIndent; --indent >= 0; ) fputs("  ", out);

  nsAutoString buf;
  mNodeInfo->GetQualifiedName(buf);
  fputs(NS_LossyConvertUCS2toASCII(buf).get(), out);

  fprintf(out, "@%p", this);

  PRUint32 index, attrcount = mAttrsAndChildren.AttrCount();
  for (index = 0; index < attrcount; index++) {
    nsAutoString buffer;

    // name
    mAttrsAndChildren.GetSafeAttrNameAt(index)->GetQualifiedName(buffer);

    // value
    buffer.AppendLiteral("=");
    nsAutoString value;
    mAttrsAndChildren.AttrAt(index)->ToString(value);
    buffer.Append(value);

    fputs(" ", out);
    fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
  }

  fprintf(out, " refcount=%d<", mRefCnt.get());

  fputs("\n", out);
  PRUint32 kids = GetChildCount();

  for (index = 0; index < kids; index++) {
    nsIContent *kid = GetChildAt(index);
    kid->List(out, aIndent + 1);
  }
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fputs(">\n", out);

  nsIDocument *document = GetOwnerDoc();
  if (document) {
    nsIBindingManager* bindingManager = document->BindingManager();
    nsCOMPtr<nsIDOMNodeList> anonymousChildren;
    bindingManager->GetAnonymousNodesFor(NS_CONST_CAST(nsGenericElement*, this),
                                         getter_AddRefs(anonymousChildren));

    if (anonymousChildren) {
      PRUint32 length;
      anonymousChildren->GetLength(&length);
      if (length) {
        for (index = aIndent; --index >= 0; ) fputs("  ", out);
        fputs("anonymous-children<\n", out);

        for (PRUint32 i = 0; i < length; ++i) {
          nsCOMPtr<nsIDOMNode> node;
          anonymousChildren->Item(i, getter_AddRefs(node));
          nsCOMPtr<nsIContent> child = do_QueryInterface(node);
          child->List(out, aIndent + 1);
        }

        for (index = aIndent; --index >= 0; ) fputs("  ", out);
        fputs(">\n", out);
      }
    }

    PRBool hasContentList;
    bindingManager->HasContentListFor(NS_CONST_CAST(nsGenericElement*, this),
                                      &hasContentList);

    if (hasContentList) {
      nsCOMPtr<nsIDOMNodeList> contentList;
      bindingManager->GetContentListFor(NS_CONST_CAST(nsGenericElement*, this),
                                        getter_AddRefs(contentList));

      NS_ASSERTION(contentList != nsnull, "oops, binding manager lied");

      PRUint32 length;
      contentList->GetLength(&length);
      if (length) {
        for (index = aIndent; --index >= 0; ) fputs("  ", out);
        fputs("content-list<\n", out);

        for (PRUint32 i = 0; i < length; ++i) {
          nsCOMPtr<nsIDOMNode> node;
          contentList->Item(i, getter_AddRefs(node));
          nsCOMPtr<nsIContent> child = do_QueryInterface(node);
          child->List(out, aIndent + 1);
        }

        for (index = aIndent; --index >= 0; ) fputs("  ", out);
        fputs(">\n", out);
      }
    }
  }
}

void
nsGenericElement::DumpContent(FILE* out, PRInt32 aIndent,
                              PRBool aDumpAll) const
{
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
nsGenericElement::IndexOf(nsIContent* aPossibleChild) const
{
  NS_PRECONDITION(nsnull != aPossibleChild, "null ptr");

  return mAttrsAndChildren.IndexOfChild(aPossibleChild);
}

void
nsGenericElement::GetContentsAsText(nsAString& aText)
{
  aText.Truncate();
  PRInt32 children = GetChildCount();

  nsCOMPtr<nsITextContent> tc;

  PRInt32 i;
  for (i = 0; i < children; ++i) {
    nsIContent *child = GetChildAt(i);

    if (child->IsContentOfType(eTEXT)) {
      tc = do_QueryInterface(child);

      tc->AppendTextTo(aText);
    }
  }
}

void*
nsGenericElement::GetProperty(nsIAtom  *aPropertyName, nsresult *aStatus) const
{
  nsIDocument *doc = GetOwnerDoc();
  if (!doc)
    return nsnull;

  return doc->PropertyTable()->GetProperty(this, aPropertyName, aStatus);
}

nsresult
nsGenericElement::SetProperty(nsIAtom            *aPropertyName,
                              void               *aValue,
                              NSPropertyDtorFunc  aDtor)
{
  nsIDocument *doc = GetOwnerDoc();
  if (!doc)
    return NS_ERROR_FAILURE;

  nsresult rv = doc->PropertyTable()->SetProperty(this, aPropertyName,
                                                  aValue, aDtor, nsnull);
  if (NS_SUCCEEDED(rv))
    SetFlags(GENERIC_ELEMENT_HAS_PROPERTIES);

  return rv;
}

nsresult
nsGenericElement::DeleteProperty(nsIAtom *aPropertyName)
{
  nsIDocument *doc = GetOwnerDoc();
  if (!doc)
    return nsnull;

  return doc->PropertyTable()->DeleteProperty(this, aPropertyName);
}

void*
nsGenericElement::UnsetProperty(nsIAtom  *aPropertyName, nsresult *aStatus)
{
  nsIDocument *doc = GetOwnerDoc();
  if (!doc)
    return nsnull;

  return doc->PropertyTable()->UnsetProperty(this, aPropertyName, aStatus);
}

nsresult
nsGenericElement::CloneNode(PRBool aDeep, nsIDOMNode **aResult) const
{
  *aResult = nsnull;

  nsIDocument *document = GetOwnerDoc();

  nsCOMPtr<nsIContent> newContent;
  nsresult rv = CloneContent(document, aDeep, getter_AddRefs(newContent));
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(newContent, aResult);
}

nsresult
nsGenericElement::CloneContent(nsIDocument *aOwnerDocument, PRBool aDeep,
                               nsIContent **aResult) const
{
  if (GetOwnerDoc() == aOwnerDocument) {
    return Clone(mNodeInfo, aDeep, aResult);
  }

  nsNodeInfoManager* nodeInfoManager = aOwnerDocument->NodeInfoManager();

  nsCOMPtr<nsINodeInfo> newNodeInfo;
  nsresult rv = nodeInfoManager->GetNodeInfo(mNodeInfo->NameAtom(),
                                             mNodeInfo->GetPrefixAtom(),
                                             mNodeInfo->NamespaceID(),
                                             getter_AddRefs(newNodeInfo));
  NS_ENSURE_SUCCESS(rv, rv);

  return Clone(newNodeInfo, aDeep, aResult);
}
