/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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
#include "nsIFrame.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
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
#include "nsScriptSecurityManager.h"
#include "nsIDOMMutationEvent.h"
#include "nsMutationEvent.h"

#include "nsIBindingManager.h"
#include "nsIXBLBinding.h"
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

#include "jsapi.h"

// baseURI
#include "nsIXMLDocument.h"
#include "nsIDOMXPathEvaluator.h"

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
  } else {
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
    } else {
      *aReturn = nsnull;
    }
  } else {
    *aReturn = nsnull;
  }

  return res;
}

void
nsChildContentList::DropReference()
{
  mContent = nsnull;
}

NS_IMETHODIMP
nsNode3Tearoff::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (aIID.Equals(NS_GET_IID(nsIDOM3Node))) {
    nsISupports *inst = this;

    NS_ADDREF(inst);

    *aInstancePtr = inst;

    return NS_OK;
  }

  return mContent->QueryInterface(aIID, aInstancePtr);
}


NS_IMPL_ADDREF(nsNode3Tearoff)
NS_IMPL_RELEASE(nsNode3Tearoff)


NS_IMETHODIMP
nsNode3Tearoff::GetBaseURI(nsAString& aURI)
{
  nsCOMPtr<nsIURI> uri;

  nsCOMPtr<nsIDocument> doc;

  mContent->GetDocument(*getter_AddRefs(doc));

  nsCOMPtr<nsIXMLDocument> xmlDoc(do_QueryInterface(doc));

  aURI.Truncate();

  if (xmlDoc) {
    // XML documents can use the XML Base (W3C spec) way of setting
    // the base per element. We look at this node and its ancestors
    // until we find the first XML content and get it's base.
    nsCOMPtr<nsIContent> content(mContent);

    while (content) {
      nsCOMPtr<nsIXMLContent> xmlContent(do_QueryInterface(content));

      if (xmlContent) {
        xmlContent->GetXMLBaseURI(getter_AddRefs(uri));

        break;
      }

      nsCOMPtr<nsIContent> tmp(content);
      tmp->GetParent(*getter_AddRefs(content));
    }
  }

  if (!uri && doc) {
    // HTML document or for some reason there was no XML content in
    // XML document

    doc->GetBaseURL(*getter_AddRefs(uri));

    if (!uri) {
      doc->GetDocumentURL(getter_AddRefs(uri));
    }
  }

  if (uri) {
    nsCAutoString spec;
    uri->GetSpec(spec);
    aURI = NS_ConvertUTF8toUCS2(spec);
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsNode3Tearoff::CompareTreePosition(nsIDOMNode* aOther,
                                    PRUint16* aReturn)
{
  NS_ENSURE_ARG_POINTER(aOther);
  PRUint16 mask = nsIDOMNode::TREE_POSITION_DISCONNECTED;

  nsCOMPtr<nsIDOMNode> node(do_QueryInterface(mContent));
  if (node == aOther) {
    mask |= nsIDOMNode::TREE_POSITION_SAME_NODE;
    nsCOMPtr<nsIDocument> doc;
    mContent->GetDocument(*getter_AddRefs(doc));
    // Loose nodes without an owner document are not equivalent
    // in tree position since they are not in any tree.
    // Add the 'equivalent' flag only if we have an owner document.
    if (doc) {
      mask |= nsIDOMNode::TREE_POSITION_EQUIVALENT;
    }
    *aReturn = mask;
    return NS_OK;
  }

  // If the other node is an attribute, document, or document fragment,
  // we can find the position easier by comparing this node relative to
  // the other node, and then reversing positions.
  PRUint16 otherType = 0;
  aOther->GetNodeType(&otherType);
  if (otherType == nsIDOMNode::ATTRIBUTE_NODE ||
      otherType == nsIDOMNode::DOCUMENT_NODE  ||
      otherType == nsIDOMNode::DOCUMENT_FRAGMENT_NODE) {
    PRUint16 otherMask = nsIDOMNode::TREE_POSITION_DISCONNECTED;
    nsCOMPtr<nsIDOM3Node> other(do_QueryInterface(aOther));
    other->CompareTreePosition(node, &otherMask);
    if (otherMask & nsIDOMNode::TREE_POSITION_FOLLOWING) {
      mask |= nsIDOMNode::TREE_POSITION_PRECEDING;
    } else if (otherMask & nsIDOMNode::TREE_POSITION_PRECEDING) {
      mask |= nsIDOMNode::TREE_POSITION_FOLLOWING;
    }

    if (otherMask & nsIDOMNode::TREE_POSITION_ANCESTOR) {
      mask |= nsIDOMNode::TREE_POSITION_DESCENDANT;
    } else if (otherMask & nsIDOMNode::TREE_POSITION_DESCENDANT) {
      mask |= nsIDOMNode::TREE_POSITION_ANCESTOR;
    }

    *aReturn = mask;
    return NS_OK;
  }

#ifdef DEBUG
  {
    PRUint16 nodeType = 0;
    node->GetNodeType(&nodeType);
    NS_ASSERTION((nodeType == nsIDOMNode::ELEMENT_NODE ||
                  nodeType == nsIDOMNode::TEXT_NODE ||
                  nodeType == nsIDOMNode::CDATA_SECTION_NODE ||
                  nodeType == nsIDOMNode::ENTITY_REFERENCE_NODE ||
                  nodeType == nsIDOMNode::PROCESSING_INSTRUCTION_NODE ||
                  nodeType == nsIDOMNode::COMMENT_NODE),
                 "Invalid node type!");
  }
#endif

  nsAutoVoidArray nodeAncestors;

  nsresult rv =
    nsContentUtils::GetFirstDifferentAncestors(node, aOther, &nodeAncestors);

  if (NS_FAILED(rv)) {
    // The nodes have no common ancestor.  We're done.
    *aReturn = mask;
    return NS_OK;
  }

  nsIDOMNode* commonAncestor =
    NS_STATIC_CAST(nsIDOMNode*, nodeAncestors.ElementAt(0));

  nsCOMPtr<nsIDocument> doc;
  mContent->GetDocument(*getter_AddRefs(doc));

  if (commonAncestor == node) {
    mask |= nsIDOMNode::TREE_POSITION_DESCENDANT;
    // Should nodes in a document fragment get following/preceding?
    // Currently we assume they can't.
    // XXXcaa - See bug 154166.
    if (doc) {
      mask |= nsIDOMNode::TREE_POSITION_FOLLOWING;
    }
    *aReturn = mask;
    return NS_OK;
  }

  if (commonAncestor == aOther) {
    mask |= nsIDOMNode::TREE_POSITION_ANCESTOR;
    // Should nodes in a document fragment get following/preceding?
    // Currently we assume they can't.
    // XXXcaa - See bug 154166.
    if (doc) {
      mask |= nsIDOMNode::TREE_POSITION_PRECEDING;
    }
    *aReturn = mask;
    return NS_OK;
  }

  if (commonAncestor && doc && nodeAncestors.Count() == 3) {
    nsIDOMNode* nodeAncestor =
      NS_STATIC_CAST(nsIDOMNode*, nodeAncestors.ElementAt(1));
    nsIDOMNode* otherAncestor =
      NS_STATIC_CAST(nsIDOMNode*, nodeAncestors.ElementAt(2));

    if (nodeAncestor && otherAncestor) {
      // Find out which of the two nodes comes first in the document order.
      // First get the children of the common ancestor.
      nsCOMPtr<nsIDOMNodeList> children;
      commonAncestor->GetChildNodes(getter_AddRefs(children));
      PRUint32 numKids;
      children->GetLength(&numKids);
      for (PRUint32 i = 0; i < numKids; ++i) {
        // Then go through the children one at a time to see which we hit first.
        nsCOMPtr<nsIDOMNode> childNode;
        children->Item(i, getter_AddRefs(childNode));
        if (childNode == nodeAncestor) {
          mask |= nsIDOMNode::TREE_POSITION_FOLLOWING;
          break;
        }

        if (childNode == otherAncestor) {
          mask |= nsIDOMNode::TREE_POSITION_PRECEDING;
          break;
        }
      }
    }
  }

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
nsNode3Tearoff::LookupNamespacePrefix(const nsAString& aNamespaceURI,
                                      nsAString& aPrefix)
{
  SetDOMStringToNull(aPrefix);

  // XXX Waiting for DOM spec to list error codes.

  nsCOMPtr<nsINameSpaceManager> manager;
  nsCOMPtr<nsINodeInfo> ni;

  mContent->GetNodeInfo(*getter_AddRefs(ni));

  if (!ni) {
    // If there's no nodeinfo (i.e. mContent is a non-element node),
    // check if the parent has nodeinfo
    nsCOMPtr<nsIContent> parent;

    mContent->GetParent(*getter_AddRefs(parent));

    if (parent) {
      parent->GetNodeInfo(*getter_AddRefs(ni));
    }
  }

  if (ni) {
    nsCOMPtr<nsINodeInfoManager> nimgr;

    ni->GetNodeInfoManager(*getter_AddRefs(nimgr));
    NS_ENSURE_TRUE(nimgr, NS_ERROR_UNEXPECTED);

    nimgr->GetNamespaceManager(*getter_AddRefs(manager));
  }

  // If there's no nodeinfo, get the manager from the document
  if (!manager) {
    nsCOMPtr<nsIDocument> doc;
    mContent->GetDocument(*getter_AddRefs(doc));

    if (doc) {
      doc->GetNameSpaceManager(*getter_AddRefs(manager));
    }
  }

  if (!manager) {
    return NS_ERROR_UNEXPECTED;
  }

  // Get the namespace id for the URI
  PRInt32 namespaceId;
  manager->GetNameSpaceID(aNamespaceURI, namespaceId);
  if (namespaceId == kNameSpaceID_Unknown) {
    return NS_OK;
  }

  nsAutoString ns;

  // Trace up the content parent chain looking for the namespace
  // declaration that defines the aNamespaceURI namespace. Once found,
  // return the prefix (i.e. the attribute localName).
  nsCOMPtr<nsIContent> content(mContent);
  while (content) {
    PRInt32 attrCount, i;

    nsCOMPtr<nsIAtom> name, prefix;
    PRInt32 namespace_id;

    content->GetAttrCount(attrCount);

    for (i = 0; i < attrCount; i++) {
      content->GetAttrNameAt(i, namespace_id, *getter_AddRefs(name),
                             *getter_AddRefs(prefix));

      if (namespace_id == kNameSpaceID_XMLNS) {
        nsresult rv = content->GetAttr(namespace_id, name, ns);

        if (rv == NS_CONTENT_ATTR_HAS_VALUE &&
            ns.Equals(aNamespaceURI)) {
          name->ToString(aPrefix);

          return NS_OK;
        }
      }
    }

    nsCOMPtr<nsIContent> tmp(content);
    tmp->GetParent(*getter_AddRefs(content));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNode3Tearoff::LookupNamespaceURI(const nsAString& aNamespacePrefix,
                                   nsAString& aNamespaceURI)
{
  nsCOMPtr<nsIAtom> name;

  if (!aNamespacePrefix.IsEmpty()) {
    name = dont_AddRef(NS_NewAtom(aNamespacePrefix));
    NS_ENSURE_TRUE(name, NS_ERROR_OUT_OF_MEMORY);
  } else {
    name = nsLayoutAtoms::xmlnsNameSpace;
  }

  // Trace up the content parent chain looking for the namespace
  // declaration that declares aNamespacePrefix.
  nsCOMPtr<nsIContent> content(mContent);

  while (content) {
    nsresult rv = content->GetAttr(kNameSpaceID_XMLNS, name, aNamespaceURI);

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
      return NS_OK;
    }

    nsCOMPtr<nsIContent> tmp(content);
    tmp->GetParent(*getter_AddRefs(content));
  }

  SetDOMStringToNull(aNamespaceURI);

  return NS_OK;
}


nsDOMEventRTTearoff *
nsDOMEventRTTearoff::mCachedEventTearoff[NS_EVENT_TEAROFF_CACHE_SIZE];

PRUint32 nsDOMEventRTTearoff::mCachedEventTearoffCount = 0;


nsDOMEventRTTearoff::nsDOMEventRTTearoff(nsIContent *aContent)
  : mContent(aContent)
{
  NS_INIT_ISUPPORTS();
}

nsDOMEventRTTearoff::~nsDOMEventRTTearoff()
{
};


NS_IMETHODIMP
nsDOMEventRTTearoff::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (aIID.Equals(NS_GET_IID(nsIDOMEventTarget)) ||
      aIID.Equals(NS_GET_IID(nsIDOMEventReceiver))) {
    nsIDOMEventReceiver *inst = this;

    NS_ADDREF(inst);

    *aInstancePtr = inst;

    return NS_OK;
  }
  else if (aIID.Equals(NS_GET_IID(nsIDOM3EventTarget))) {
    nsIDOM3EventTarget *inst = this;

    NS_ADDREF(inst);

    *aInstancePtr = inst;

    return NS_OK;
  }

  return mContent->QueryInterface(aIID, aInstancePtr);
}


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
  if (NS_SUCCEEDED(GetListenerManager(getter_AddRefs(manager))) && manager) {
    return manager->GetSystemEventGroupLM(aGroup);
  }
  return NS_ERROR_FAILURE;
}

// nsIDOMEventTarget
NS_IMETHODIMP
nsDOMEventRTTearoff::AddEventListener(const nsAString& type,
                                      nsIDOMEventListener *listener,
                                      PRBool useCapture)
{
  nsCOMPtr<nsIDOMEventReceiver> event_receiver;
  nsresult rv = GetEventReceiver(getter_AddRefs(event_receiver));
  NS_ENSURE_SUCCESS(rv, rv);

  return event_receiver->AddEventListener(type, listener, useCapture);
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

  return event_target->AddGroupedEventListener(aType, aListener, aUseCapture, aEvtGrp);
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

  return event_target->RemoveGroupedEventListener(aType, aListener, aUseCapture, aEvtGrp);
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

//----------------------------------------------------------------------

nsDOMSlots::nsDOMSlots(PtrBits aFlags)
  : mFlags(aFlags & ~GENERIC_ELEMENT_CONTENT_ID_MASK), mChildNodes(nsnull),
    mStyle(nsnull), mAttributeMap(nsnull), mBindingParent(nsnull),
    mContentID(aFlags >> GENERIC_ELEMENT_CONTENT_ID_BITS_OFFSET)
{
}

nsDOMSlots::~nsDOMSlots()
{
  if (mChildNodes) {
    mChildNodes->DropReference();
    NS_RELEASE(mChildNodes);
  }

  if (mStyle) {
    mStyle->DropReference();
    NS_RELEASE(mStyle);
  }

  if (mAttributeMap) {
    mAttributeMap->DropReference();
    NS_RELEASE(mAttributeMap);
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

nsGenericElement::nsGenericElement()
  : mDocument(nsnull), mParent(nsnull), mNodeInfo(nsnull),
    mFlagsOrSlots(GENERIC_ELEMENT_DOESNT_HAVE_DOMSLOTS)
{
  NS_INIT_ISUPPORTS();
}

nsGenericElement::~nsGenericElement()
{
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

  NS_IF_RELEASE(mNodeInfo);

  if (HasDOMSlots()) {
    nsDOMSlots *slots = GetDOMSlots();

    delete slots;
  }

  // No calling GetFlags() beond this point...
}

PR_STATIC_CALLBACK(void)
RangeListHashInitEntry(PLDHashTable *table, PLDHashEntryHdr *entry,
                       const void *key)
{
  // Initialize the entry with placement new
  new (entry) RangeListMapEntry(key);
}

PR_STATIC_CALLBACK(void)
RangeListHashClearEntry(PLDHashTable *table, PLDHashEntryHdr *entry)
{
  RangeListMapEntry *r = NS_STATIC_CAST(RangeListMapEntry *, entry);

  // Let the RangeListMapEntry clean itself up...
  r->~RangeListMapEntry();
}

PR_STATIC_CALLBACK(void)
EventListenerManagerHashInitEntry(PLDHashTable *table, PLDHashEntryHdr *entry,
                                  const void *key)
{
  // Initialize the entry with placement new
  new (entry) EventListenerManagerMapEntry(key);
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

nsresult
nsGenericElement::Init(nsINodeInfo *aNodeInfo)
{
  NS_ENSURE_ARG(aNodeInfo);

  mNodeInfo = aNodeInfo;
  NS_ADDREF(mNodeInfo);

  nsresult rv = NS_OK;

  if (!sRangeListsHash.ops) {
    rv = InitHashes();
  }

  return rv;
}

NS_IMETHODIMP
nsGenericElement::GetNodeName(nsAString& aNodeName)
{
  return mNodeInfo->GetQualifiedName(aNodeName);
}

NS_IMETHODIMP
nsGenericElement::GetLocalName(nsAString& aLocalName)
{
  return mNodeInfo->GetLocalName(aLocalName);
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
  if (mParent) {
    return CallQueryInterface(mParent, aParentNode);
  }

  if (mDocument) {
    // If we don't have a parent, but we're in the document, we must
    // be the root node of the document. The DOM says that the root
    // is the document.

    return CallQueryInterface(mDocument, aParentNode);
  }

  *aParentNode = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetPreviousSibling(nsIDOMNode** aPrevSibling)
{
  nsIContent* sibling = nsnull;
  nsresult result = NS_OK;

  if (mParent) {
    PRInt32 pos;
    mParent->IndexOf(this, pos);
    if (pos > 0 ) {
      mParent->ChildAt(--pos, sibling);
    }
  } else if (mDocument) {
    // Nodes that are just below the document (their parent is the
    // document) need to go to the document to find their next sibling.
    PRInt32 pos;
    mDocument->IndexOf(this, pos);
    if (pos > 0 ) {
      mDocument->ChildAt(--pos, sibling);
    }
  }

  if (sibling) {
    result = sibling->QueryInterface(NS_GET_IID(nsIDOMNode),
                                     (void**)aPrevSibling);
    NS_ASSERTION(NS_OK == result, "Must be a DOM Node");
    NS_RELEASE(sibling); // balance the AddRef in ChildAt()
  } else {
    *aPrevSibling = nsnull;
  }

  return result;
}

NS_IMETHODIMP
nsGenericElement::GetNextSibling(nsIDOMNode** aNextSibling)
{
  nsIContent* sibling = nsnull;
  nsresult result = NS_OK;

  if (mParent) {
    PRInt32 pos;
    mParent->IndexOf(this, pos);
    if (pos > -1 ) {
      mParent->ChildAt(++pos, sibling);
    }
  } else if (mDocument) {
    // Nodes that are just below the document (their parent is the
    // document) need to go to the document to find their next sibling.
    PRInt32 pos;
    mDocument->IndexOf(this, pos);
    if (pos > -1 ) {
      mDocument->ChildAt(++pos, sibling);
    }
  }

  if (sibling) {
    result = sibling->QueryInterface(NS_GET_IID(nsIDOMNode),
                                     (void**)aNextSibling);
    NS_ASSERTION(NS_OK == result, "Must be a DOM Node");
    NS_RELEASE(sibling); // balance the AddRef in ChildAt()
  } else {
    *aNextSibling = nsnull;
  }

  return result;
}

NS_IMETHODIMP
nsGenericElement::GetOwnerDocument(nsIDOMDocument** aOwnerDocument)
{
  NS_ENSURE_ARG_POINTER(aOwnerDocument);

  nsCOMPtr<nsIDocument> doc(mDocument);

  if (!doc) {
    // If we're not part of the document we can check if our nodeinfo
    // can get at the document
    mNodeInfo->GetDocument(*getter_AddRefs(doc));
  }

  if (doc) {
    return CallQueryInterface(doc, aOwnerDocument);
  }

  // No document, return nsnull
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
  return mNodeInfo->GetPrefix(aPrefix);
}

NS_IMETHODIMP
nsGenericElement::SetPrefix(const nsAString& aPrefix)
{
  // XXX: Validate the prefix string!

  nsINodeInfo *newNodeInfo = nsnull;
  nsCOMPtr<nsIAtom> prefix;

  if (!aPrefix.IsEmpty() && !DOMStringIsNull(aPrefix)) {
    prefix = dont_AddRef(NS_NewAtom(aPrefix));
    NS_ENSURE_TRUE(prefix, NS_ERROR_OUT_OF_MEMORY);
  }

  nsresult rv = mNodeInfo->PrefixChanged(prefix, newNodeInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_RELEASE(mNodeInfo);

  mNodeInfo = newNodeInfo;

  return NS_OK;
}

extern PRBool gCheckedForXPathDOM;
extern PRBool gHaveXPathDOM;

nsresult
nsGenericElement::InternalIsSupported(const nsAString& aFeature,
                                      const nsAString& aVersion,
                                      PRBool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = PR_FALSE;
  nsAutoString feature(aFeature);

  if (feature.Equals(NS_LITERAL_STRING("XML"),
                     nsCaseInsensitiveStringComparator()) ||
      feature.Equals(NS_LITERAL_STRING("HTML"),
                     nsCaseInsensitiveStringComparator())) {
    if (aVersion.IsEmpty() ||
        aVersion.Equals(NS_LITERAL_STRING("1.0")) ||
        aVersion.Equals(NS_LITERAL_STRING("2.0"))) {
      *aReturn = PR_TRUE;
    }
  } else if (feature.Equals(NS_LITERAL_STRING("Views"),
                            nsCaseInsensitiveStringComparator()) ||
             feature.Equals(NS_LITERAL_STRING("StyleSheets"),
                            nsCaseInsensitiveStringComparator()) ||
             feature.Equals(NS_LITERAL_STRING("Core"),
                            nsCaseInsensitiveStringComparator()) ||
             feature.Equals(NS_LITERAL_STRING("CSS"),
                            nsCaseInsensitiveStringComparator()) ||
             feature.Equals(NS_LITERAL_STRING("CSS2"),
                            nsCaseInsensitiveStringComparator()) ||
             feature.Equals(NS_LITERAL_STRING("Events"),
                            nsCaseInsensitiveStringComparator()) ||
//           feature.Equals(NS_LITERAL_STRING("UIEvents"),
//                          nsCaseInsensitiveStringComparator()) ||
             feature.Equals(NS_LITERAL_STRING("MouseEvents"),
                            nsCaseInsensitiveStringComparator()) ||
             feature.Equals(NS_LITERAL_STRING("MouseScrollEvents"),
                            nsCaseInsensitiveStringComparator()) ||
             feature.Equals(NS_LITERAL_STRING("HTMLEvents"),
                            nsCaseInsensitiveStringComparator()) ||
             feature.Equals(NS_LITERAL_STRING("Range"),
                            nsCaseInsensitiveStringComparator())) {
    if (aVersion.IsEmpty() || aVersion.Equals(NS_LITERAL_STRING("2.0"))) {
      *aReturn = PR_TRUE;
    }
  } else if ((!gCheckedForXPathDOM || gHaveXPathDOM) &&
             feature.Equals(NS_LITERAL_STRING("XPath"),
                            nsCaseInsensitiveStringComparator()) &&
             (aVersion.IsEmpty() ||
              aVersion.Equals(NS_LITERAL_STRING("3.0")))) {
    if (!gCheckedForXPathDOM) {
      nsCOMPtr<nsIDOMXPathEvaluator> evaluator;
      evaluator = do_CreateInstance(NS_XPATH_EVALUATOR_CONTRACTID);
      gHaveXPathDOM = (evaluator != nsnull);
      gCheckedForXPathDOM = PR_TRUE;
    }
    *aReturn = gHaveXPathDOM;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::IsSupported(const nsAString& aFeature,
                              const nsAString& aVersion,
                              PRBool* aReturn)
{
  return InternalIsSupported(aFeature, aVersion, aReturn);
}

NS_IMETHODIMP
nsGenericElement::HasAttributes(PRBool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  PRInt32 attrCount = 0;

  GetAttrCount(attrCount);

  *aReturn = !!attrCount;

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetXMLBaseURI(nsIURI **aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);

  *aURI = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetAttributes(nsIDOMNamedNodeMap** aAttributes)
{
  NS_PRECONDITION(nsnull != aAttributes, "null pointer argument");
  nsDOMSlots *slots = GetDOMSlots();

  if (nsnull == slots->mAttributeMap) {
    slots->mAttributeMap = new nsDOMAttributeMap(this);
    if (!slots->mAttributeMap) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(slots->mAttributeMap);
  }

  return CallQueryInterface(slots->mAttributeMap, aAttributes);
}

NS_IMETHODIMP
nsGenericElement::GetTagName(nsAString& aTagName)
{
  mNodeInfo->GetName(aTagName);

  return NS_OK;
}

nsresult
nsGenericElement::GetAttribute(const nsAString& aName,
                               nsAString& aReturn)
{
  nsCOMPtr<nsINodeInfo> ni;
  NormalizeAttrString(aName, *getter_AddRefs(ni));
  NS_ENSURE_TRUE(ni, NS_ERROR_FAILURE);

  PRInt32 nsid;
  nsCOMPtr<nsIAtom> nameAtom;

  ni->GetNamespaceID(nsid);
  ni->GetNameAtom(*getter_AddRefs(nameAtom));

  nsresult rv = GetAttr(nsid, nameAtom, aReturn);

  if (rv == NS_CONTENT_ATTR_NOT_THERE) {
    SetDOMStringToNull(aReturn);
  }

  return NS_OK;
}

nsresult
nsGenericElement::SetAttribute(const nsAString& aName,
                               const nsAString& aValue)
{
  nsCOMPtr<nsINodeInfo> ni;
  NormalizeAttrString(aName, *getter_AddRefs(ni));

  return SetAttr(ni, aValue, PR_TRUE);
}

nsresult
nsGenericElement::RemoveAttribute(const nsAString& aName)
{
  nsCOMPtr<nsINodeInfo> ni;
  NormalizeAttrString(aName, *getter_AddRefs(ni));
  NS_ENSURE_TRUE(ni, NS_ERROR_FAILURE);

  PRInt32 nsid;
  nsCOMPtr<nsIAtom> tag;

  ni->GetNamespaceID(nsid);
  ni->GetNameAtom(*getter_AddRefs(tag));

  return UnsetAttr(nsid, tag, PR_TRUE);
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
  nsCOMPtr<nsIAtom> nameAtom;

  nameAtom = dont_AddRef(NS_NewAtom(aTagname));
  NS_ENSURE_TRUE(nameAtom, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIContentList> list;
  NS_GetContentList(mDocument, nameAtom, kNameSpaceID_Unknown, this,
                    getter_AddRefs(list));

  if (!list) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(list, aReturn);
}

nsresult
nsGenericElement::GetAttributeNS(const nsAString& aNamespaceURI,
                                 const nsAString& aLocalName,
                                 nsAString& aReturn)
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

  GetAttr(nsid, name, aReturn);

  return NS_OK;
}

nsresult
nsGenericElement::SetAttributeNS(const nsAString& aNamespaceURI,
                                 const nsAString& aQualifiedName,
                                 const nsAString& aValue)
{
  nsCOMPtr<nsINodeInfoManager> nimgr;
  mNodeInfo->GetNodeInfoManager(*getter_AddRefs(nimgr));
  NS_ENSURE_TRUE(nimgr, NS_ERROR_FAILURE);

  nsCOMPtr<nsINodeInfo> ni;
  nsresult rv = nimgr->GetNodeInfo(aQualifiedName, aNamespaceURI,
                                   *getter_AddRefs(ni));
  NS_ENSURE_SUCCESS(rv, rv);

  return SetAttr(ni, aValue, PR_TRUE);
}

nsresult
nsGenericElement::RemoveAttributeNS(const nsAString& aNamespaceURI,
                                    const nsAString& aLocalName)
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
  nsCOMPtr<nsIAtom> nameAtom(dont_AddRef(NS_NewAtom(aLocalName)));
  NS_ENSURE_TRUE(nameAtom, NS_ERROR_OUT_OF_MEMORY);

  PRInt32 nameSpaceId = kNameSpaceID_Unknown;

  nsCOMPtr<nsIContentList> list;

  if (!aNamespaceURI.Equals(NS_LITERAL_STRING("*"))) {
    nsCOMPtr<nsINodeInfoManager> nimgr;
    mNodeInfo->GetNodeInfoManager(*getter_AddRefs(nimgr));
    NS_ENSURE_TRUE(nimgr, NS_ERROR_FAILURE);

    nsCOMPtr<nsINameSpaceManager> nsmgr;
    nimgr->GetNamespaceManager(*getter_AddRefs(nsmgr));
    NS_ENSURE_TRUE(nsmgr, NS_ERROR_FAILURE);

    nsmgr->GetNameSpaceID(aNamespaceURI, nameSpaceId);

    if (nameSpaceId == kNameSpaceID_Unknown) {
      // Unknown namespace means no matches, we create an empty list...
      NS_GetContentList(mDocument, nsnull, kNameSpaceID_None, nsnull,
                        getter_AddRefs(list));
      NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);
    }
  }

  if (!list) {
    NS_GetContentList(mDocument, nameAtom, nameSpaceId, this,
                      getter_AddRefs(list));
    NS_ENSURE_TRUE(list, NS_ERROR_OUT_OF_MEMORY);
  }

  return CallQueryInterface(list, aReturn);
}

nsresult
nsGenericElement::HasAttribute(const nsAString& aName, PRBool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);

  nsCOMPtr<nsINodeInfo> ni;
  NormalizeAttrString(aName, *getter_AddRefs(ni));
  NS_ENSURE_TRUE(ni, NS_ERROR_FAILURE);

  PRInt32 nsid;
  nsCOMPtr<nsIAtom> nameAtom;

  ni->GetNamespaceID(nsid);
  ni->GetNameAtom(*getter_AddRefs(nameAtom));

  *aReturn = HasAttr(nsid, nameAtom);
  return NS_OK;
}

nsresult
nsGenericElement::HasAttributeNS(const nsAString& aNamespaceURI,
                                 const nsAString& aLocalName,
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
  aResult = mDocument;
  NS_IF_ADDREF(aResult);

  return NS_OK;
}


void
nsGenericElement::SetDocumentInChildrenOf(nsIContent* aContent,
                                          nsIDocument* aDocument,
                                          PRBool aCompileEventHandlers)
{
  nsCOMPtr<nsIContent> child;
  PRInt32 i, n;
  aContent->ChildCount(n);

  for (i = 0; i < n; i++) {
    aContent->ChildAt(i, *getter_AddRefs(child));

    if (child) {
      child->SetDocument(aDocument, PR_TRUE, aCompileEventHandlers);
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

    if (aDocument) {
      // check the document on the nodeinfo to see whether we need a
      // new nodeinfo
      nsCOMPtr<nsIDocument> nodeinfoDoc;
      mNodeInfo->GetDocument(*getter_AddRefs(nodeinfoDoc));
      if (aDocument != nodeinfoDoc) {
        // get a new nodeinfo
        nsCOMPtr<nsIAtom> name;
        mNodeInfo->GetNameAtom(*getter_AddRefs(name));
        nsCOMPtr<nsIAtom> prefix;
        mNodeInfo->GetPrefixAtom(*getter_AddRefs(prefix));
        PRInt32 nameSpaceID;
        mNodeInfo->GetNamespaceID(nameSpaceID);
        nsCOMPtr<nsINodeInfoManager> nodeInfoManager;
        aDocument->GetNodeInfoManager(*getter_AddRefs(nodeInfoManager));
        if (nodeInfoManager) {
          nsINodeInfo* newNodeInfo;
          nodeInfoManager->GetNodeInfo(name, prefix, nameSpaceID,
                                       newNodeInfo);
          if (newNodeInfo) {
            NS_RELEASE(mNodeInfo);
            mNodeInfo = newNodeInfo;
          }
        }
      }
    }

    mDocument = aDocument;
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
  if (aParent) {
    nsCOMPtr<nsIContent> bindingPar;
    aParent->GetBindingParent(getter_AddRefs(bindingPar));
    if (bindingPar)
      SetBindingParent(bindingPar);
  }
  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
nsGenericElement::IsNativeAnonymous() const
{
  return !!(GetFlags() & GENERIC_ELEMENT_IS_ANONYMOUS);
}

NS_IMETHODIMP_(void)
nsGenericElement::SetNativeAnonymous(PRBool aAnonymous)
{
  if (aAnonymous) {
    SetFlags(GENERIC_ELEMENT_IS_ANONYMOUS);
  } else {
    UnsetFlags(GENERIC_ELEMENT_IS_ANONYMOUS);
  }
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
  NS_ADDREF(aResult);

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

  // Find out if we're anonymous.
  nsCOMPtr<nsIContent> bindingParent;
  if (*aDOMEvent) {
    (*aDOMEvent)->GetTarget(getter_AddRefs(oldTarget));
    nsCOMPtr<nsIContent> content(do_QueryInterface(oldTarget));
    if (content)
      content->GetBindingParent(getter_AddRefs(bindingParent));
  } else {
    GetBindingParent(getter_AddRefs(bindingParent));
  }

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

  // determine the parent:
  nsCOMPtr<nsIContent> parent;
  if (mDocument) {
    nsCOMPtr<nsIBindingManager> bindingManager;
    mDocument->GetBindingManager(getter_AddRefs(bindingManager));
    if (bindingManager) {
      // we have a binding manager -- do we have an anonymous parent?
      bindingManager->GetInsertionParent(this, getter_AddRefs(parent));
    }
  }
  if (parent) {
    retarget = PR_FALSE;
  } else {
    // if we didn't find an anonymous parent, use the explicit one,
    // whether it's null or not...
    parent = mParent;
  }

  if (retarget || (parent.get() != mParent)) {
    if (!*aDOMEvent) {
      // We haven't made a DOMEvent yet.  Force making one now.
      nsCOMPtr<nsIEventListenerManager> listenerManager;
      if (NS_FAILED(ret = GetListenerManager(getter_AddRefs(listenerManager)))) {
        return ret;
      }
      nsAutoString empty;
      if (NS_FAILED(ret = listenerManager->CreateEvent(aPresContext, aEvent,
                                                       empty, aDOMEvent)))
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
      nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(mParent);
      privateEvent->SetTarget(target);
    }
  }

  //Capturing stage evaluation
  if (NS_EVENT_FLAG_CAPTURE & aFlags && aEvent->message != NS_PAGE_LOAD &&
      aEvent->message != NS_SCRIPT_LOAD &&
      aEvent->message != NS_IMAGE_ERROR && aEvent->message != NS_IMAGE_LOAD &&
      aEvent->message != NS_SCROLL_EVENT) {
    //Initiate capturing phase.  Special case first call to document
    if (parent) {
      parent->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                             aFlags & NS_EVENT_CAPTURE_MASK,
                             aEventStatus);
    } else if (mDocument != nsnull) {
        ret = mDocument->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                        aFlags & NS_EVENT_CAPTURE_MASK,
                                        aEventStatus);
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
      do_QueryInterface(NS_STATIC_CAST(nsIHTMLContent *, this));

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
      nsCOMPtr<nsIDOMEventTarget> parentTarget(do_QueryInterface(mParent));
      privateEvent->SetTarget(parentTarget);
    }
  }

  //Bubbling stage
  if (NS_EVENT_FLAG_BUBBLE & aFlags && mDocument &&
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

      ret = mDocument->HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                                      aFlags & NS_EVENT_BUBBLE_MASK, 
                                      aEventStatus);
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
  }

  return ret;
}

#ifdef DEBUG
PRUint32
nsGenericElement::BaseSizeOf(nsISizeOfHandler *aSizer) const
{
  return 0;
}
#endif

NS_IMETHODIMP
nsGenericElement::GetContentID(PRUint32* aID)
{
  nsDOMSlots *slots = GetExistingDOMSlots();

  if (slots) {
    *aID = slots->mContentID;
  } else {
    PtrBits flags = GetFlags();

    *aID = flags >> GENERIC_ELEMENT_CONTENT_ID_BITS_OFFSET;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::SetContentID(PRUint32 aID)
{
  if (HasDOMSlots() || aID > GENERIC_ELEMENT_CONTENT_ID_MAX_VALUE) {
    nsDOMSlots *slots = GetDOMSlots();

    if (!slots) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    slots->mContentID = aID;
  } else {
    UnsetFlags(GENERIC_ELEMENT_CONTENT_ID_MASK);
    SetFlags(aID << GENERIC_ELEMENT_CONTENT_ID_BITS_OFFSET);
  }

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
  return NS_OK;
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

NS_IMETHODIMP
nsGenericElement::GetInlineStyleRule(nsIStyleRule** aStyleRule)
{
  *aStyleRule = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::GetMappedAttributeImpact(const nsIAtom* aAttribute, PRInt32 aModType, 
                                           nsChangeHint& aHint) const
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
nsGenericElement::GetAttributeMappingFunction(nsMapRuleToAttributesFunc& aMapRuleFunc) const
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGenericElement::AttributeToString(nsIAtom* aAttribute,
                                    const nsHTMLValue& aValue,
                                    nsAString& aResult) const
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGenericElement::StringToAttribute(nsIAtom* aAttribute,
                                    const nsAString& aValue,
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
nsGenericElement::GetBaseTarget(nsAString& aBaseTarget) const
{
  return NS_ERROR_NOT_IMPLEMENTED;
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


nsresult
nsGenericElement::RangeRemove(nsIDOMRange* aRange)
{
  if (!HasRangeList()) {
    return NS_ERROR_UNEXPECTED;
  }

  RangeListMapEntry *entry =
    NS_STATIC_CAST(RangeListMapEntry *,
                   PL_DHashTableOperate(&sRangeListsHash, this,
                                        PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_FREE(entry)) {
    NS_ERROR("Huh, our bit says we have a range list, but there's nothing "
             "in the hash!?!!");

    return NS_ERROR_UNEXPECTED;
  }

  if (!entry->mRangeList) {
    return NS_ERROR_UNEXPECTED;
  }

  // dont need to release - this call is made by the range object itself
  PRBool rv = entry->mRangeList->RemoveElement(aRange);
  if (!rv) {
    return NS_ERROR_FAILURE;
  }

  if (entry->mRangeList->Count() == 0) {
    PL_DHashTableRawRemove(&sRangeListsHash, entry);

    UnsetFlags(GENERIC_ELEMENT_HAS_RANGELIST);
  }

  return NS_OK;
}


nsresult
nsGenericElement::GetRangeList(nsVoidArray*& aResult) const
{
  aResult = nsnull;

  if (!HasRangeList()) {
    return NS_OK;
  }

  RangeListMapEntry *entry =
    NS_STATIC_CAST(RangeListMapEntry *,
                   PL_DHashTableOperate(&sRangeListsHash, this,
                                        PL_DHASH_LOOKUP));

  if (PL_DHASH_ENTRY_IS_FREE(entry)) {
    NS_ERROR("Huh, our bit says we have a range list, but there's nothing "
             "in the hash!?!!");

    return NS_ERROR_UNEXPECTED;
  }

  aResult = entry->mRangeList;

  return NS_OK;
}

nsresult
nsGenericElement::SetFocus(nsIPresContext* aPresContext)
{
  if (HasAttr(kNameSpaceID_None, nsHTMLAtoms::disabled)) {
    return NS_OK;
  }
 
  nsCOMPtr<nsIEventStateManager> esm;
  aPresContext->GetEventStateManager(getter_AddRefs(esm));

  if (esm) {
    esm->SetContentState(this, NS_EVENT_STATE_FOCUS);
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
  nsDOMSlots *slots = GetExistingDOMSlots();

  if (slots) {
    *aContent = slots->mBindingParent;

    NS_IF_ADDREF(*aContent);
  } else {
    *aContent = nsnull;
  }

  return NS_OK;
}

nsresult
nsGenericElement::SetBindingParent(nsIContent* aParent)
{
  nsDOMSlots *slots = GetDOMSlots();

  if (!slots) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  slots->mBindingParent = aParent; // Weak, so no addref happens.

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

NS_IMETHODIMP_(PRBool)
nsGenericElement::IsContentOfType(PRUint32 aFlags)
{
  return !(aFlags & ~eELEMENT);
}

//----------------------------------------------------------------------

nsresult
nsGenericElement::GetListenerManager(nsIEventListenerManager** aResult)
{
  *aResult = nsnull;

  if (!sEventListenerManagersHash.ops) {
    // We''re already shut down, don't bother creating a event
    // listener manager.

    return NS_OK;
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

    entry->mListenerManager->SetListenerTarget(NS_STATIC_CAST(nsIHTMLContent *,
                                                              this));

    SetFlags(GENERIC_ELEMENT_HAS_LISTENERMANAGER);
  }

  *aResult = entry->mListenerManager;
  NS_ADDREF(*aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsGenericElement::DoneCreatingElement()
{
  return NS_OK;
}

//----------------------------------------------------------------------

// Generic DOMNode implementations

/*
 * This helper function checks if aChild is the same as aNode or if
 * aChild is one of aNode's ancestors. -- jst@citec.fi
 */

static PRBool
isSelfOrAncestor(nsIContent *aNode, nsIContent *aChild)
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

  nsCOMPtr<nsIDocument> old_doc;
  newContent->GetDocument(*getter_AddRefs(old_doc));
  if (old_doc && old_doc != mDocument &&
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
   * Check if this is a document fragment. If it is, we need
   * to remove the children of the document fragment and add them
   * individually (i.e. we don't add the actual document fragment).
   */
  if (nodeType == nsIDOMNode::DOCUMENT_FRAGMENT_NODE) {
    nsCOMPtr<nsIContent> childContent;
    PRInt32 count, old_count, i;

    nsCOMPtr<nsIDocumentFragment> doc_fragment(do_QueryInterface(newContent));
    NS_ENSURE_TRUE(doc_fragment, NS_ERROR_UNEXPECTED);

    doc_fragment->DisconnectChildren();

    newContent->ChildCount(count);

    ChildCount(old_count);

    PRBool do_notify = !!aRefChild;

    if (count && !do_notify && mDocument) {
      mDocument->BeginUpdate();
    }    
    
    /*
     * Iterate through the fragments children, removing each from
     * the fragment and inserting it into the child list of its
     * new parent.
     */
    for (i = 0; i < count; i++) {
      // Get the n:th child from the document fragment. Since we
      // disconnected the children from the document fragment they
      // won't be removed from the document fragment when inserted
      // into the new parent. This lets us do this operation *much*
      // faster.
      res = newContent->ChildAt(i, *getter_AddRefs(childContent));
      if (NS_FAILED(res)) {
        break;
      }

      // Insert the child and increment the insertion position
      res = InsertChildAt(childContent, refPos++, do_notify, PR_TRUE);

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

    if (count && !do_notify && mDocument) {
      mDocument->ContentAppended(this, old_count);
      mDocument->EndUpdate();
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

    nsContentUtils::ReparentContentWrapper(newContent, this, mDocument,
                                           old_doc);

    res = InsertChildAt(newContent, refPos, PR_TRUE, PR_TRUE);

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

  nsCOMPtr<nsIDocument> old_doc;
  newContent->GetDocument(*getter_AddRefs(old_doc));
  if (old_doc && old_doc != mDocument &&
      !nsContentUtils::CanCallerAccess(aNewChild)) {
    return NS_ERROR_DOM_SECURITY_ERR;
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

      // Insert the child and increment the insertion position
      if (i) {
        res = InsertChildAt(childContent, oldPos++, PR_TRUE, PR_TRUE);
      } else {
        res = ReplaceChildAt(childContent, oldPos++, PR_TRUE, PR_TRUE);
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

    nsContentUtils::ReparentContentWrapper(newContent, this, mDocument,
                                           old_doc);

    if (aNewChild == aOldChild) {
      // We're replacing a child with itself. In this case the child
      // has already been removed from this element once we get here
      // so we can't call ReplaceChildAt() (since aOldChild is no
      // longer at oldPos). In stead we'll call InsertChildAt() to put
      // the child back where it was.

      res = InsertChildAt(newContent, oldPos, PR_TRUE, PR_TRUE);
    } else {
      res = ReplaceChildAt(newContent, oldPos, PR_TRUE, PR_TRUE);
    }

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
  } else if (aIID.Equals(NS_GET_IID(nsIDOM3Node))) {
    inst = new nsNode3Tearoff(this);
    NS_ENSURE_TRUE(inst, NS_ERROR_OUT_OF_MEMORY);
  } else if (aIID.Equals(NS_GET_IID(nsIDOMEventReceiver)) ||
             aIID.Equals(NS_GET_IID(nsIDOMEventTarget))) {
    inst = NS_STATIC_CAST(nsIDOMEventReceiver *,
                          nsDOMEventRTTearoff::Create(this));
    NS_ENSURE_TRUE(inst, NS_ERROR_OUT_OF_MEMORY);
  } else if (aIID.Equals(NS_GET_IID(nsIDOM3EventTarget))) {
    inst = NS_STATIC_CAST(nsIDOM3EventTarget *,
                          nsDOMEventRTTearoff::Create(this));
    NS_ENSURE_TRUE(inst, NS_ERROR_OUT_OF_MEMORY);
  } else {
    return NS_NOINTERFACE;
  }

  NS_ADDREF(inst);

  *aInstancePtr = inst;

  return NS_OK;
}

NS_IMPL_ADDREF(nsGenericElement)
NS_IMPL_RELEASE(nsGenericElement)

nsresult
nsGenericElement::PostQueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (mDocument) {
    nsCOMPtr<nsIBindingManager> manager;
    mDocument->GetBindingManager(getter_AddRefs(manager));
    if (manager)
      return manager->GetBindingImplementation(this, aIID, aInstancePtr);
  }

  return NS_NOINTERFACE;
}

//----------------------------------------------------------------------
nsresult
nsGenericElement::LeaveLink(nsIPresContext* aPresContext)
{
  nsCOMPtr<nsILinkHandler> handler;
  nsresult rv = aPresContext->GetLinkHandler(getter_AddRefs(handler));
  if (NS_FAILED(rv) || (nsnull == handler)) {
    return rv;
  }

  return handler->OnLeaveLink();
}

nsresult
nsGenericElement::TriggerLink(nsIPresContext* aPresContext,
                              nsLinkVerb aVerb,
                              nsIURI* aBaseURL,
                              const nsAString& aURLSpec,
                              const nsAFlatString& aTargetSpec,
                              PRBool aClick)
{
  nsCOMPtr<nsILinkHandler> handler;
  nsresult rv = aPresContext->GetLinkHandler(getter_AddRefs(handler));
  if (NS_FAILED(rv) || !handler) return rv;

  // Resolve url to an absolute url
  nsCOMPtr<nsIURI> targetURI;
  nsAutoString docCharset;
  if (mDocument &&
      NS_SUCCEEDED(mDocument->GetDocumentCharacterSet(docCharset))) {
    rv = NS_NewURI(getter_AddRefs(targetURI), aURLSpec,
                   NS_LossyConvertUCS2toASCII(docCharset).get(), aBaseURL);
  } else {
    rv = NS_NewURI(getter_AddRefs(targetURI), aURLSpec, nsnull, aBaseURL);
  }

  NS_ENSURE_SUCCESS(rv, rv);

  // Now pass on absolute url to the click handler
  if (aClick) {
    nsresult proceed = NS_OK;
    // Check that this page is allowed to load this URI.
    nsCOMPtr<nsIScriptSecurityManager> securityManager = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
      proceed =
        securityManager->CheckLoadURI(aBaseURL, targetURI,
                                      nsIScriptSecurityManager::STANDARD);

    // Only pass off the click event if the script security manager
    // says it's ok.
    if (NS_SUCCEEDED(proceed))
      handler->OnLinkClick(this, aVerb, targetURI,
                           aTargetSpec.get());
  } else {
    handler->OnOverLink(this, targetURI,
                        aTargetSpec.get());
  }
  return rv;
}

nsresult
nsGenericElement::AddScriptEventListener(nsIAtom* aAttribute,
                                         const nsAString& aValue)
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

    if (!cx) {
      stack->GetSafeJSContext(&cx);
      NS_ENSURE_TRUE(cx, NS_ERROR_FAILURE);
    }

    nsContentUtils::GetDynamicScriptContext(cx, getter_AddRefs(context));
    NS_ENSURE_TRUE(context, NS_ERROR_FAILURE);
  }

  //Attributes on the body and frameset tags get set on the global object
  if (mNodeInfo->Equals(nsHTMLAtoms::body) ||
      mNodeInfo->Equals(nsHTMLAtoms::frameset)) {
    if (!global && cx) {
      nsContentUtils::GetDynamicScriptGlobal(cx, getter_AddRefs(global));

      NS_ENSURE_TRUE(global, NS_ERROR_FAILURE);
    }
    nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(global));
    NS_ENSURE_TRUE(receiver, NS_ERROR_FAILURE);

    nsCOMPtr<nsIEventListenerManager> manager;

    receiver->GetListenerManager(getter_AddRefs(manager));

    if (manager) {
      ret = manager->AddScriptEventListener(context, global, aAttribute,
                                            aValue, PR_FALSE);
    }
  } else {
    nsCOMPtr<nsIEventListenerManager> manager;
    GetListenerManager(getter_AddRefs(manager));

    if (manager) {
      ret = manager->AddScriptEventListener(context, this, aAttribute, aValue,
                                            PR_TRUE);
    }
  }

  return ret;
}


//----------------------------------------------------------------------

struct nsGenericAttribute
{
  nsGenericAttribute(nsINodeInfo *aNodeInfo, const nsAString& aValue)
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
nsGenericContainerElement::NormalizeAttrString(const nsAString& aStr, nsINodeInfo*& aNodeInfo)
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
      result = aDst->SetAttr(attr->mNodeInfo, attr->mValue, PR_FALSE);
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
              result = aDst->AppendChildTo(newContent, PR_FALSE, PR_FALSE);
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

  if (!slots->mChildNodes) {
    slots->mChildNodes = new nsChildContentList(this);
    if (!slots->mChildNodes) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(slots->mChildNodes);
  }

  return CallQueryInterface(slots->mChildNodes, aChildNodes);
}

nsresult
nsGenericContainerElement::HasChildNodes(PRBool* aReturn)
{
  if (0 != mChildren.Count()) {
    *aReturn = PR_TRUE;
  } else {
    *aReturn = PR_FALSE;
  }
  return NS_OK;
}

nsresult
nsGenericContainerElement::GetFirstChild(nsIDOMNode** aNode)
{
  nsIContent *child = (nsIContent *)mChildren.SafeElementAt(0);
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
  if (mChildren.Count() != 0) {
    nsIContent *child = (nsIContent *)mChildren.ElementAt(mChildren.Count()-1);
    if (nsnull != child) {
      nsresult res = child->QueryInterface(NS_GET_IID(nsIDOMNode),
                                           (void**)aNode);
      NS_ASSERTION(NS_OK == res, "Must be a DOM Node"); // must be a DOM Node
      return res;
    }
  }
  *aNode = nsnull;
  return NS_OK;
}

nsresult
nsGenericContainerElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                   const nsAString& aValue,
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

  return SetAttr(ni, aValue, aNotify);
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
nsGenericContainerElement::SetAttr(nsINodeInfo* aNodeInfo,
                                   const nsAString& aValue,
                                   PRBool aNotify)
{
  NS_ENSURE_ARG_POINTER(aNodeInfo);

  PRBool modification = PR_FALSE;
  nsAutoString oldValue;

  nsresult rv = NS_ERROR_OUT_OF_MEMORY;

  if (!mAttributes) {
    mAttributes = new nsAutoVoidArray();
    NS_ENSURE_TRUE(mAttributes, NS_ERROR_OUT_OF_MEMORY);
  }

  nsCOMPtr<nsIAtom> name;
  PRInt32 nameSpaceID;

  aNodeInfo->GetNameAtom(*getter_AddRefs(name));
  aNodeInfo->GetNamespaceID(nameSpaceID);

  if (aNotify && (nsnull != mDocument)) {
    mDocument->BeginUpdate();

    mDocument->AttributeWillChange(this, nameSpaceID, name);
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
    nsCOMPtr<nsIBindingManager> bindingManager;
    mDocument->GetBindingManager(getter_AddRefs(bindingManager));
    nsCOMPtr<nsIXBLBinding> binding;
    bindingManager->GetBinding(this, getter_AddRefs(binding));
    if (binding)
      binding->AttributeChanged(name, nameSpaceID, PR_FALSE, aNotify);

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
      if (!oldValue.IsEmpty()) {
        mutation.mPrevAttrValue = getter_AddRefs(NS_NewAtom(oldValue));
      }

      if (!aValue.IsEmpty()) {
        mutation.mNewAttrValue = getter_AddRefs(NS_NewAtom(aValue));
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
      mDocument->AttributeChanged(this, nameSpaceID, name, modHint, 
                                  NS_STYLE_HINT_UNKNOWN);
      mDocument->EndUpdate();
    }
  }

  return rv;
}

nsresult
nsGenericContainerElement::GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                   nsAString& aResult) const
{
  nsCOMPtr<nsIAtom> prefix;
  return GetAttr(aNameSpaceID, aName, *getter_AddRefs(prefix), aResult);
}

nsresult
nsGenericContainerElement::GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                   nsIAtom*& aPrefix,
                                   nsAString& aResult) const
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
        if (!aResult.IsEmpty()) {
          rv = NS_CONTENT_ATTR_HAS_VALUE;
        } else {
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

NS_IMETHODIMP_(PRBool)
nsGenericContainerElement::HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const
{
  NS_ASSERTION(nsnull != aName, "must have attribute name");
  if (nsnull == aName)
    return PR_FALSE;
  
  if (nsnull != mAttributes) {
    PRInt32 count = mAttributes->Count();
    PRInt32 index;
    for (index = 0; index < count; index++) {
      const nsGenericAttribute* attr = (const nsGenericAttribute*)mAttributes->ElementAt(index);
      if ((aNameSpaceID == kNameSpaceID_Unknown ||
           attr->mNodeInfo->NamespaceEquals(aNameSpaceID)) &&
          (attr->mNodeInfo->Equals(aName))) {
        return PR_TRUE;
      }
    }
  }

  return PR_FALSE;
}

nsresult
nsGenericContainerElement::UnsetAttr(PRInt32 aNameSpaceID,
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

          mDocument->AttributeWillChange(this, aNameSpaceID, aName);
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
          this->HandleDOMEvent(nsnull, &mutation, nsnull,
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
        binding->AttributeChanged(aName, aNameSpaceID, PR_TRUE, aNotify);

      if (aNotify) {
        mDocument->AttributeChanged(this, aNameSpaceID, aName, nsIDOMMutationEvent::REMOVAL, 
                                    NS_STYLE_HINT_UNKNOWN);
        mDocument->EndUpdate();
      }
    }
  }

  return rv;
}

nsresult
nsGenericContainerElement::GetAttrNameAt(PRInt32 aIndex,
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
nsGenericContainerElement::GetAttrCount(PRInt32& aResult) const
{
  if (nsnull != mAttributes) {
    aResult = mAttributes->Count();
  } else {
    aResult = 0;
  }
  return NS_OK;
}

#ifdef DEBUG
void
nsGenericContainerElement::ListAttributes(FILE* out) const
{
  PRInt32 index, count;
  GetAttrCount(count);

  for (index = 0; index < count; index++) {
    const nsGenericAttribute* attr = (const nsGenericAttribute*)mAttributes->ElementAt(index);
    nsAutoString buffer;

    // name
    nsAutoString name;
    attr->mNodeInfo->GetQualifiedName(name);
    buffer.Append(name);

    // value
    buffer.Append(NS_LITERAL_STRING("="));
    buffer.Append(attr->mValue);

    fputs(" ", out);
    fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
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
  fputs(NS_LossyConvertUCS2toASCII(buf).get(), out);

  fprintf(out, "@%p", this);

  ListAttributes(out);

  fprintf(out, " refcount=%d<", mRefCnt.get());

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

  if (mDocument) {
    nsCOMPtr<nsIBindingManager> bindingManager;
    mDocument->GetBindingManager(getter_AddRefs(bindingManager));
    if (bindingManager) {
      nsCOMPtr<nsIDOMNodeList> anonymousChildren;
      bindingManager->GetAnonymousNodesFor(NS_STATIC_CAST(nsIContent*, NS_CONST_CAST(nsGenericContainerElement*, this)),
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
      bindingManager->HasContentListFor(NS_STATIC_CAST(nsIContent*, NS_CONST_CAST(nsGenericContainerElement*, this)),
                                        &hasContentList);

      if (hasContentList) {
        nsCOMPtr<nsIDOMNodeList> contentList;
        bindingManager->GetContentListFor(NS_STATIC_CAST(nsIContent*, NS_CONST_CAST(nsGenericContainerElement*, this)),
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

  return NS_OK;
}

nsresult
nsGenericContainerElement::DumpContent(FILE* out, PRInt32 aIndent,
                                       PRBool aDumpAll) const {
  return NS_OK;
}
#endif

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
  // I really prefer NOT to do this test on all ChildAt calls - perhaps we
  // should add FastChildAt().
  nsIContent *child = (nsIContent *)mChildren.SafeElementAt(aIndex);
  NS_IF_ADDREF(child);
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
                                         PRBool aNotify,
                                         PRBool aDeepSetDocument)
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
      aKid->SetDocument(doc, aDeepSetDocument, PR_TRUE);
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
        aKid->HandleDOMEvent(nsnull, &mutation, nsnull,
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
                                          PRBool aNotify,
                                          PRBool aDeepSetDocument)
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
      aKid->SetDocument(doc, aDeepSetDocument, PR_TRUE);
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
nsGenericContainerElement::AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                         PRBool aDeepSetDocument)
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
      aKid->SetDocument(doc, aDeepSetDocument, PR_TRUE);
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
        aKid->HandleDOMEvent(nsnull, &mutation, nsnull,
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
      oldKid->HandleDOMEvent(nsnull, &mutation, nsnull,
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

#ifdef DEBUG
PRUint32
nsGenericContainerElement::BaseSizeOf(nsISizeOfHandler *aSizer) const
{
  PRUint32 sum = 0;
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

  return sum;
}
#endif
