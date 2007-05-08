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
 * The Original Code is Mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *         Jonas Sicking <jonas@sicking.cc> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsNodeUtils.h"
#include "nsContentUtils.h"
#include "nsINode.h"
#include "nsIContent.h"
#include "nsIMutationObserver.h"
#include "nsIDocument.h"
#include "nsIDOMUserDataHandler.h"
#include "nsIEventListenerManager.h"
#include "nsIAttribute.h"
#include "nsIXPConnect.h"
#include "nsGenericElement.h"
#include "pldhash.h"
#include "nsIDOMAttr.h"
#include "nsCOMArray.h"
#include "nsPIDOMWindow.h"
#ifdef MOZ_XUL
#include "nsXULElement.h"
#endif

#define IMPL_MUTATION_NOTIFICATION(func_, content_, params_)      \
  PR_BEGIN_MACRO                                                  \
  nsINode* node = content_;                                       \
  nsINode* prev;                                                  \
  do {                                                            \
    nsINode::nsSlots* slots = node->GetExistingSlots();           \
    if (slots && !slots->mMutationObservers.IsEmpty()) {          \
      /* No need to explicitly notify the first observer first    \
         since that'll happen anyway. */                          \
      NS_OBSERVER_ARRAY_NOTIFY_OBSERVERS(                         \
        slots->mMutationObservers, nsIMutationObserver,           \
        func_, params_);                                          \
    }                                                             \
    prev = node;                                                  \
    node = node->GetNodeParent();                                 \
                                                                  \
    if (!node && prev->IsNodeOfType(nsINode::eXUL)) {             \
      /* XUL elements can have the in-document flag set, but      \
         still be in an orphaned subtree. In this case we         \
         need to notify the document */                           \
      node = NS_STATIC_CAST(nsIContent*, prev)->GetCurrentDoc();  \
    }                                                             \
  } while (node);                                                 \
  PR_END_MACRO


void
nsNodeUtils::CharacterDataChanged(nsIContent* aContent,
                                  CharacterDataChangeInfo* aInfo)
{
  nsIDocument* doc = aContent->GetOwnerDoc();
  IMPL_MUTATION_NOTIFICATION(CharacterDataChanged, aContent,
                             (doc, aContent, aInfo));
}

void
nsNodeUtils::AttributeChanged(nsIContent* aContent,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType)
{
  nsIDocument* doc = aContent->GetOwnerDoc();
  IMPL_MUTATION_NOTIFICATION(AttributeChanged, aContent,
                             (doc, aContent, aNameSpaceID, aAttribute,
                              aModType));
}

void
nsNodeUtils::ContentAppended(nsIContent* aContainer,
                             PRInt32 aNewIndexInContainer)
{
  nsIDocument* document = aContainer->GetOwnerDoc();

  IMPL_MUTATION_NOTIFICATION(ContentAppended, aContainer,
                             (document, aContainer, aNewIndexInContainer));
}

void
nsNodeUtils::ContentInserted(nsINode* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer)
{
  NS_PRECONDITION(aContainer->IsNodeOfType(nsINode::eCONTENT) ||
                  aContainer->IsNodeOfType(nsINode::eDOCUMENT),
                  "container must be an nsIContent or an nsIDocument");
  nsIContent* container;
  nsIDocument* document;
  if (aContainer->IsNodeOfType(nsINode::eCONTENT)) {
    container = NS_STATIC_CAST(nsIContent*, aContainer);
    document = aContainer->GetOwnerDoc();
  }
  else {
    container = nsnull;
    document = NS_STATIC_CAST(nsIDocument*, aContainer);
  }

  IMPL_MUTATION_NOTIFICATION(ContentInserted, aContainer,
                             (document, container, aChild, aIndexInContainer));
}

void
nsNodeUtils::ContentRemoved(nsINode* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer)
{
  NS_PRECONDITION(aContainer->IsNodeOfType(nsINode::eCONTENT) ||
                  aContainer->IsNodeOfType(nsINode::eDOCUMENT),
                  "container must be an nsIContent or an nsIDocument");
  nsIContent* container;
  nsIDocument* document;
  if (aContainer->IsNodeOfType(nsINode::eCONTENT)) {
    container = NS_STATIC_CAST(nsIContent*, aContainer);
    document = aContainer->GetOwnerDoc();
  }
  else {
    container = nsnull;
    document = NS_STATIC_CAST(nsIDocument*, aContainer);
  }

  IMPL_MUTATION_NOTIFICATION(ContentRemoved, aContainer,
                             (document, container, aChild, aIndexInContainer));
}

void
nsNodeUtils::ParentChainChanged(nsIContent *aContent)
{
  // No need to notify observers on the parents since their parent
  // chain must have been changed too and so their observers were
  // notified at that time.

  nsINode::nsSlots* slots = aContent->GetExistingSlots();
  if (slots && !slots->mMutationObservers.IsEmpty()) {
    NS_OBSERVER_ARRAY_NOTIFY_OBSERVERS(
        slots->mMutationObservers,
        nsIMutationObserver,
        ParentChainChanged,
        (aContent));
  }
}

struct nsHandlerData
{
  PRUint16 mOperation;
  nsCOMPtr<nsIDOMNode> mSource, mDest;
};

static void
CallHandler(void *aObject, nsIAtom *aKey, void *aHandler, void *aData)
{
  nsHandlerData *handlerData = NS_STATIC_CAST(nsHandlerData*, aData);
  nsCOMPtr<nsIDOMUserDataHandler> handler =
    NS_STATIC_CAST(nsIDOMUserDataHandler*, aHandler);

  nsINode *node = NS_STATIC_CAST(nsINode*, aObject);
  nsCOMPtr<nsIVariant> data =
    NS_STATIC_CAST(nsIVariant*, node->GetProperty(DOM_USER_DATA, aKey));
  NS_ASSERTION(data, "Handler without data?");

  nsAutoString key;
  aKey->ToString(key);
  handler->Handle(handlerData->mOperation, key, data, handlerData->mSource,
                  handlerData->mDest);
}

void
nsNodeUtils::LastRelease(nsINode* aNode, PRBool aDelete)
{
  nsINode::nsSlots* slots = aNode->GetExistingSlots();
  if (slots) {
    if (!slots->mMutationObservers.IsEmpty()) {
      NS_OBSERVER_ARRAY_NOTIFY_OBSERVERS(slots->mMutationObservers,
                                         nsIMutationObserver,
                                         NodeWillBeDestroyed, (aNode));
    }

    PtrBits flags = slots->mFlags | NODE_DOESNT_HAVE_SLOTS;
    delete slots;
    aNode->mFlagsOrSlots = flags;
  }

  // Kill properties first since that may run external code, so we want to
  // be in as complete state as possible at that time.
  if (aNode->HasProperties()) {
    // Strong reference to the document so that deleting properties can't
    // delete the document.
    nsCOMPtr<nsIDocument> document = aNode->GetOwnerDoc();
    if (document) {
      nsHandlerData handlerData;
      handlerData.mOperation = nsIDOMUserDataHandler::NODE_DELETED;

      nsPropertyTable *table = document->PropertyTable();

      table->Enumerate(aNode, DOM_USER_DATA_HANDLER, CallHandler, &handlerData);
      table->DeleteAllPropertiesFor(aNode);
    }
    aNode->UnsetFlags(NODE_HAS_PROPERTIES);
  }

  if (aNode->HasFlag(NODE_HAS_LISTENERMANAGER)) {
#ifdef DEBUG
    if (nsContentUtils::IsInitialized()) {
      nsCOMPtr<nsIEventListenerManager> manager;
      nsContentUtils::GetListenerManager(aNode, PR_FALSE,
                                         getter_AddRefs(manager));
      if (!manager) {
        NS_ERROR("Huh, our bit says we have a listener manager list, "
                 "but there's nothing in the hash!?!!");
      }
    }
#endif

    nsContentUtils::RemoveListenerManager(aNode);
    aNode->UnsetFlags(NODE_HAS_LISTENERMANAGER);
  }

  if (aDelete) {
    delete aNode;
  }
}

/* static */
nsresult
nsNodeUtils::CallUserDataHandlers(nsCOMArray<nsINode> &aNodesWithProperties,
                                  nsIDocument *aOwnerDocument,
                                  PRUint16 aOperation, PRBool aCloned)
{
  NS_PRECONDITION(!aCloned || (aNodesWithProperties.Count() % 2 == 0),
                  "Expected aNodesWithProperties to contain original and "
                  "cloned nodes.");

  nsPropertyTable *table = aOwnerDocument->PropertyTable();

  // Keep the document alive, just in case one of the handlers causes it to go
  // away.
  nsCOMPtr<nsIDocument> ownerDoc = aOwnerDocument;

  nsHandlerData handlerData;
  handlerData.mOperation = aOperation;

  PRUint32 i, count = aNodesWithProperties.Count();
  for (i = 0; i < count; ++i) {
    nsINode *nodeWithProperties = aNodesWithProperties[i];

    nsresult rv;
    handlerData.mSource = do_QueryInterface(nodeWithProperties, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    if (aCloned) {
      handlerData.mDest = do_QueryInterface(aNodesWithProperties[++i], &rv);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    table->Enumerate(nodeWithProperties, DOM_USER_DATA_HANDLER, CallHandler,
                     &handlerData);
  }

  return NS_OK;
}

/* static */
nsresult
nsNodeUtils::CloneNodeImpl(nsINode *aNode, PRBool aDeep, nsIDOMNode **aResult)
{
  *aResult = nsnull;

  nsCOMPtr<nsIDOMNode> newNode;
  nsCOMArray<nsINode> nodesWithProperties;
  nsresult rv = Clone(aNode, aDeep, nsnull, nodesWithProperties,
                      getter_AddRefs(newNode));
  NS_ENSURE_SUCCESS(rv, rv);

  nsIDocument *ownerDoc = aNode->GetOwnerDoc();
  if (ownerDoc) {
    rv = CallUserDataHandlers(nodesWithProperties, ownerDoc,
                              nsIDOMUserDataHandler::NODE_CLONED, PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  newNode.swap(*aResult);

  return NS_OK;
}

class AdoptFuncData {
public:
  AdoptFuncData(nsIDOMElement *aElement, nsNodeInfoManager *aNewNodeInfoManager,
                JSContext *aCx, JSObject *aOldScope, JSObject *aNewScope,
                nsCOMArray<nsINode> &aNodesWithProperties)
    : mElement(aElement),
      mNewNodeInfoManager(aNewNodeInfoManager),
      mCx(aCx),
      mOldScope(aOldScope),
      mNewScope(aNewScope),
      mNodesWithProperties(aNodesWithProperties)
  {
  }

  nsIDOMElement *mElement;
  nsNodeInfoManager *mNewNodeInfoManager;
  JSContext *mCx;
  JSObject *mOldScope;
  JSObject *mNewScope;
  nsCOMArray<nsINode> &mNodesWithProperties;
};

PLDHashOperator PR_CALLBACK
AdoptFunc(nsAttrHashKey::KeyType aKey, nsIDOMNode *aData, void* aUserArg)
{
  nsCOMPtr<nsIAttribute> attr = do_QueryInterface(aData);
  NS_ASSERTION(attr, "non-nsIAttribute somehow made it into the hashmap?!");

  AdoptFuncData *data = NS_STATIC_CAST(AdoptFuncData*, aUserArg);

  // If we were passed an element we need to clone the attribute nodes and
  // insert them into the element.
  PRBool clone = data->mElement != nsnull;
  nsCOMPtr<nsIDOMNode> node;
  nsresult rv = nsNodeUtils::CloneAndAdopt(attr, clone, PR_TRUE,
                                           data->mNewNodeInfoManager,
                                           data->mCx, data->mOldScope,
                                           data->mNewScope,
                                           data->mNodesWithProperties,
                                           nsnull, getter_AddRefs(node));

  if (NS_SUCCEEDED(rv) && clone) {
    nsCOMPtr<nsIDOMAttr> dummy, attribute = do_QueryInterface(node, &rv);
    if (NS_SUCCEEDED(rv)) {
      rv = data->mElement->SetAttributeNode(attribute, getter_AddRefs(dummy));
    }
  }

  return NS_SUCCEEDED(rv) ? PL_DHASH_NEXT : PL_DHASH_STOP;
}

/* static */
nsresult
nsNodeUtils::CloneAndAdopt(nsINode *aNode, PRBool aClone, PRBool aDeep,
                           nsNodeInfoManager *aNewNodeInfoManager,
                           JSContext *aCx, JSObject *aOldScope,
                           JSObject *aNewScope,
                           nsCOMArray<nsINode> &aNodesWithProperties,
                           nsINode *aParent, nsIDOMNode **aResult)
{
  NS_PRECONDITION((!aClone && aNewNodeInfoManager) || !aCx,
                  "If cloning or not getting a new nodeinfo we shouldn't "
                  "rewrap");
  NS_PRECONDITION(!aCx || (aOldScope && aNewScope), "Must have scopes");
  NS_PRECONDITION(!aParent || !aNode->IsNodeOfType(nsINode::eDOCUMENT),
                  "Can't insert document nodes into a parent");

  *aResult = nsnull;

  // First deal with aNode and walk its attributes (and their children). Then,
  // if aDeep is PR_TRUE, deal with aNode's children (and recurse into their
  // attributes and children).

  nsresult rv;
  nsNodeInfoManager *nodeInfoManager = aNewNodeInfoManager;

  // aNode.
  nsINodeInfo *nodeInfo = aNode->mNodeInfo;
  nsCOMPtr<nsINodeInfo> newNodeInfo;
  if (nodeInfoManager) {
    rv = nodeInfoManager->GetNodeInfo(nodeInfo->NameAtom(),
                                      nodeInfo->GetPrefixAtom(),
                                      nodeInfo->NamespaceID(),
                                      getter_AddRefs(newNodeInfo));
    NS_ENSURE_SUCCESS(rv, rv);

    nodeInfo = newNodeInfo;
  }

  nsGenericElement *elem = aNode->IsNodeOfType(nsINode::eELEMENT) ?
                           NS_STATIC_CAST(nsGenericElement*, aNode) :
                           nsnull;

  nsCOMPtr<nsINode> clone;
  if (aClone) {
    rv = aNode->Clone(nodeInfo, getter_AddRefs(clone));
    NS_ENSURE_SUCCESS(rv, rv);

    if (aParent) {
      // If we're cloning we need to insert the cloned children into the cloned
      // parent.
      nsCOMPtr<nsIContent> cloneContent = do_QueryInterface(clone, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aParent->AppendChildTo(cloneContent, PR_FALSE);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else if (aDeep && clone->IsNodeOfType(nsINode::eDOCUMENT)) {
      // After cloning the document itself, we want to clone the children into
      // the cloned document (somewhat like cloning and importing them into the
      // cloned document).
      nodeInfoManager = clone->mNodeInfo->NodeInfoManager();
    }
  }
  else if (nodeInfoManager) {
    nsCOMPtr<nsISupports> oldRef;
    nsIDocument* oldDoc = aNode->GetOwnerDoc();
    if (oldDoc) {
      oldRef = oldDoc->GetReference(aNode);
      if (oldRef) {
        oldDoc->RemoveReference(aNode);
      }
    }

    aNode->mNodeInfo.swap(newNodeInfo);

    nsIDocument* newDoc = aNode->GetOwnerDoc();
    if (newDoc) {
      if (oldRef) {
        newDoc->AddReference(aNode, oldRef);
      }

      nsPIDOMWindow* window = newDoc->GetInnerWindow();
      if (window) {
        nsCOMPtr<nsIEventListenerManager> elm;
        aNode->GetListenerManager(PR_FALSE, getter_AddRefs(elm));
        if (elm) {
          window->SetMutationListeners(elm->MutationListenerBits());
        }
      }
    }

    if (elem) {
      elem->RecompileScriptEventListeners();
    }

    if (aCx) {
      nsIXPConnect *xpc = nsContentUtils::XPConnect();
      if (xpc) {
        nsCOMPtr<nsIXPConnectJSObjectHolder> oldWrapper;
        rv = xpc->ReparentWrappedNativeIfFound(aCx, aOldScope, aNewScope, aNode,
                                               getter_AddRefs(oldWrapper));
        if (NS_FAILED(rv)) {
          aNode->mNodeInfo.swap(nodeInfo);

          return rv;
        }
      }
    }
  }

  if (elem) {
    // aNode's attributes.
    const nsDOMAttributeMap *map = elem->GetAttributeMap();
    if (map) {
      nsCOMPtr<nsIDOMElement> element;
      if (aClone) {
        // If we're cloning we need to insert the cloned attribute nodes into
        // the cloned element.
        element = do_QueryInterface(clone, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      AdoptFuncData data(element, nodeInfoManager, aCx, aOldScope,
                         aNewScope, aNodesWithProperties);

      PRUint32 count = map->Enumerate(AdoptFunc, &data);
      NS_ENSURE_TRUE(count == map->Count(), NS_ERROR_FAILURE);
    }
  }

  // The DOM spec says to always adopt/clone/import the children of attribute
  // nodes.
  // XXX The following block is here because our implementation of attribute
  //     nodes is broken when it comes to inserting children. Instead of cloning
  //     their children we force creation of the only child by calling
  //     GetChildAt(0). We can remove this when
  //     https://bugzilla.mozilla.org/show_bug.cgi?id=56758 is fixed.
  if (aClone && aNode->IsNodeOfType(nsINode::eATTRIBUTE)) {
    nsCOMPtr<nsINode> attrChildNode = aNode->GetChildAt(0);
    // We only need to do this if the child node has properties (because we
    // might need to call a userdata handler).
    if (attrChildNode && attrChildNode->HasProperties()) {
      nsCOMPtr<nsINode> clonedAttrChildNode = clone->GetChildAt(0);
      if (clonedAttrChildNode) {
        PRBool ok = aNodesWithProperties.AppendObject(attrChildNode) &&
                    aNodesWithProperties.AppendObject(clonedAttrChildNode);
        NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);
      }
    }
  }
  // XXX End of workaround for broken attribute nodes.
  else if (aDeep || aNode->IsNodeOfType(nsINode::eATTRIBUTE)) {
    // aNode's children.
    PRUint32 i, length = aNode->GetChildCount();
    for (i = 0; i < length; ++i) {
      nsCOMPtr<nsIDOMNode> child;
      rv = CloneAndAdopt(aNode->GetChildAt(i), aClone, PR_TRUE, nodeInfoManager,
                         aCx, aOldScope, aNewScope, aNodesWithProperties,
                         clone, getter_AddRefs(child));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // XXX setting document on some nodes not in a document so XBL will bind
  // and chrome won't break. Make XBL bind to document-less nodes!
  // XXXbz Once this is fixed, fix up the asserts in all implementations of
  // BindToTree to assert what they would like to assert, and fix the
  // ChangeDocumentFor() call in nsXULElement::BindToTree as well.  Also,
  // remove the UnbindFromTree call in ~nsXULElement, and add back in the
  // precondition in nsXULElement::UnbindFromTree and remove the line in
  // nsXULElement.h that makes nsNodeUtils a friend of nsXULElement.
  // Note: Make sure to do this witchery _after_ we've done any deep
  // cloning, so kids of the new node aren't confused about whether they're
  // in a document.
#ifdef MOZ_XUL
  if (aClone && !aParent && aNode->IsNodeOfType(nsINode::eXUL)) {
    nsXULElement *xulElem = NS_STATIC_CAST(nsXULElement*, elem);
    if (!xulElem->mPrototype || xulElem->IsInDoc()) {
      clone->mParentPtrBits |= nsINode::PARENT_BIT_INDOCUMENT;
    }
  }
#endif

  if (aNode->HasProperties()) {
    PRBool ok = aNodesWithProperties.AppendObject(aNode);
    if (aClone) {
      ok = ok && aNodesWithProperties.AppendObject(clone);
    }

    NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);
  }

  return clone ? CallQueryInterface(clone, aResult) : NS_OK;
}
