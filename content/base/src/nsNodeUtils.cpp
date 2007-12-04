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

// This macro expects the ownerDocument of content_ to be in scope as
// |nsIDocument* doc|
#define IMPL_MUTATION_NOTIFICATION(func_, content_, params_)      \
  PR_BEGIN_MACRO                                                  \
  nsINode* node = content_;                                       \
  NS_ASSERTION(node->GetOwnerDoc() == doc, "Bogus document");     \
  if (doc) {                                                      \
    static_cast<nsIMutationObserver*>(doc->BindingManager())->    \
      func_ params_;                                              \
  }                                                               \
  do {                                                            \
    nsINode::nsSlots* slots = node->GetExistingSlots();           \
    if (slots && !slots->mMutationObservers.IsEmpty()) {          \
      /* No need to explicitly notify the first observer first    \
         since that'll happen anyway. */                          \
      NS_OBSERVER_ARRAY_NOTIFY_OBSERVERS(                         \
        slots->mMutationObservers, nsIMutationObserver,           \
        func_, params_);                                          \
    }                                                             \
    node = node->GetNodeParent();                                 \
  } while (node);                                                 \
  PR_END_MACRO


void
nsNodeUtils::CharacterDataWillChange(nsIContent* aContent,
                                     CharacterDataChangeInfo* aInfo)
{
  nsIDocument* doc = aContent->GetOwnerDoc();
  IMPL_MUTATION_NOTIFICATION(CharacterDataWillChange, aContent,
                             (doc, aContent, aInfo));
}

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
                              PRInt32 aModType,
                              PRUint32 aStateMask)
{
  nsIDocument* doc = aContent->GetOwnerDoc();
  IMPL_MUTATION_NOTIFICATION(AttributeChanged, aContent,
                             (doc, aContent, aNameSpaceID, aAttribute,
                              aModType, aStateMask));
}

void
nsNodeUtils::ContentAppended(nsIContent* aContainer,
                             PRInt32 aNewIndexInContainer)
{
  nsIDocument* doc = aContainer->GetOwnerDoc();

  IMPL_MUTATION_NOTIFICATION(ContentAppended, aContainer,
                             (doc, aContainer, aNewIndexInContainer));
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
  nsIDocument* doc = aContainer->GetOwnerDoc();
  nsIDocument* document;
  if (aContainer->IsNodeOfType(nsINode::eCONTENT)) {
    container = static_cast<nsIContent*>(aContainer);
    document = doc;
  }
  else {
    container = nsnull;
    document = static_cast<nsIDocument*>(aContainer);
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
  nsIDocument* doc = aContainer->GetOwnerDoc();
  nsIDocument* document;
  if (aContainer->IsNodeOfType(nsINode::eCONTENT)) {
    container = static_cast<nsIContent*>(aContainer);
    document = doc;
  }
  else {
    container = nsnull;
    document = static_cast<nsIDocument*>(aContainer);
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

void
nsNodeUtils::LastRelease(nsINode* aNode)
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
  if (aNode->IsNodeOfType(nsINode::eDOCUMENT)) {
    // Delete all properties before tearing down the document. Some of the
    // properties are bound to nsINode objects and the destructor functions of
    // the properties may want to use the owner document of the nsINode.
    static_cast<nsIDocument*>(aNode)->PropertyTable()->DeleteAllProperties();
  }
  else if (aNode->HasProperties()) {
    // Strong reference to the document so that deleting properties can't
    // delete the document.
    nsCOMPtr<nsIDocument> document = aNode->GetOwnerDoc();
    if (document) {
      document->PropertyTable()->DeleteAllPropertiesFor(aNode);
    }
  }
  aNode->UnsetFlags(NODE_HAS_PROPERTIES);

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

  delete aNode;
}

static nsresult
SetUserDataProperty(PRUint16 aCategory, nsINode *aNode, nsIAtom *aKey,
                    nsISupports* aValue, void** aOldValue)
{
  nsresult rv = aNode->SetProperty(aCategory, aKey, aValue,
                                   nsPropertyTable::SupportsDtorFunc, PR_TRUE,
                                   aOldValue);
  NS_ENSURE_SUCCESS(rv, rv);

  // Property table owns it now.
  NS_ADDREF(aValue);

  return NS_OK;
}

/* static */
nsresult
nsNodeUtils::SetUserData(nsINode *aNode, const nsAString &aKey,
                         nsIVariant *aData, nsIDOMUserDataHandler *aHandler,
                         nsIVariant **aResult)
{
  *aResult = nsnull;

  nsCOMPtr<nsIAtom> key = do_GetAtom(aKey);
  if (!key) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv;
  void *data;
  if (aData) {
    rv = SetUserDataProperty(DOM_USER_DATA, aNode, key, aData, &data);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    data = aNode->UnsetProperty(DOM_USER_DATA, key);
  }

  // Take over ownership of the old data from the property table.
  nsCOMPtr<nsIVariant> oldData = dont_AddRef(static_cast<nsIVariant*>(data));

  if (aData && aHandler) {
    nsCOMPtr<nsIDOMUserDataHandler> oldHandler;
    rv = SetUserDataProperty(DOM_USER_DATA_HANDLER, aNode, key, aHandler,
                             getter_AddRefs(oldHandler));
    if (NS_FAILED(rv)) {
      // We failed to set the handler, remove the data.
      aNode->DeleteProperty(DOM_USER_DATA, key);

      return rv;
    }
  }
  else {
    aNode->DeleteProperty(DOM_USER_DATA_HANDLER, key);
  }

  oldData.swap(*aResult);

  return NS_OK;
}

/* static */
nsresult
nsNodeUtils::GetUserData(nsINode *aNode, const nsAString &aKey,
                         nsIVariant **aResult)
{
  nsCOMPtr<nsIAtom> key = do_GetAtom(aKey);
  if (!key) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  *aResult = static_cast<nsIVariant*>
                        (aNode->GetProperty(DOM_USER_DATA, key));
  NS_IF_ADDREF(*aResult);

  return NS_OK;
}

struct nsHandlerData
{
  PRUint16 mOperation;
  nsCOMPtr<nsIDOMNode> mSource, mDest;
};

static void
CallHandler(void *aObject, nsIAtom *aKey, void *aHandler, void *aData)
{
  nsHandlerData *handlerData = static_cast<nsHandlerData*>(aData);
  nsCOMPtr<nsIDOMUserDataHandler> handler =
    static_cast<nsIDOMUserDataHandler*>(aHandler);
  nsINode *node = static_cast<nsINode*>(aObject);
  nsCOMPtr<nsIVariant> data =
    static_cast<nsIVariant*>(node->GetProperty(DOM_USER_DATA, aKey));
  NS_ASSERTION(data, "Handler without data?");

  nsAutoString key;
  aKey->ToString(key);
  handler->Handle(handlerData->mOperation, key, data, handlerData->mSource,
                  handlerData->mDest);
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

static void
NoteUserData(void *aObject, nsIAtom *aKey, void *aXPCOMChild, void *aData)
{
  nsCycleCollectionTraversalCallback* cb =
    static_cast<nsCycleCollectionTraversalCallback*>(aData);
  cb->NoteXPCOMChild(static_cast<nsISupports*>(aXPCOMChild));
}

/* static */
void
nsNodeUtils::TraverseUserData(nsINode* aNode,
                              nsCycleCollectionTraversalCallback &aCb)
{
  nsIDocument* ownerDoc = aNode->GetOwnerDoc();
  if (!ownerDoc) {
    return;
  }

  nsPropertyTable *table = ownerDoc->PropertyTable();

  table->Enumerate(aNode, DOM_USER_DATA, NoteUserData, &aCb);
  table->Enumerate(aNode, DOM_USER_DATA_HANDLER, NoteUserData, &aCb);
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

  AdoptFuncData *data = static_cast<AdoptFuncData*>(aUserArg);

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
                           static_cast<nsGenericElement*>(aNode) :
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
    nsXULElement *xulElem = static_cast<nsXULElement*>(elem);
    if (!xulElem->mPrototype || xulElem->IsInDoc()) {
      clone->SetFlags(NODE_FORCE_XBL_BINDINGS);
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


/* static */
void
nsNodeUtils::UnlinkUserData(nsINode *aNode)
{
  NS_ASSERTION(aNode->HasProperties(), "Call to UnlinkUserData not needed.");

  // Strong reference to the document so that deleting properties can't
  // delete the document.
  nsCOMPtr<nsIDocument> document = aNode->GetOwnerDoc();
  if (document) {
    document->PropertyTable()->DeleteAllPropertiesFor(aNode, DOM_USER_DATA);
    document->PropertyTable()->DeleteAllPropertiesFor(aNode,
                                                      DOM_USER_DATA_HANDLER);
  }
}
