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
 * The Initial Developer of the Original Code is Mozilla Foundation.
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
#include "mozilla/dom/Element.h"
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
#include "nsDocument.h"
#ifdef MOZ_XUL
#include "nsXULElement.h"
#endif
#include "nsBindingManager.h"
#include "nsGenericHTMLElement.h"
#ifdef MOZ_MEDIA
#include "nsHTMLMediaElement.h"
#endif // MOZ_MEDIA
#include "nsImageLoadingContent.h"
#include "jsobj.h"
#include "jsgc.h"

using namespace mozilla::dom;

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
nsNodeUtils::AttributeWillChange(Element* aElement,
                                 PRInt32 aNameSpaceID,
                                 nsIAtom* aAttribute,
                                 PRInt32 aModType)
{
  nsIDocument* doc = aElement->GetOwnerDoc();
  IMPL_MUTATION_NOTIFICATION(AttributeWillChange, aElement,
                             (doc, aElement, aNameSpaceID, aAttribute,
                              aModType));
}

void
nsNodeUtils::AttributeChanged(Element* aElement,
                              PRInt32 aNameSpaceID,
                              nsIAtom* aAttribute,
                              PRInt32 aModType)
{
  nsIDocument* doc = aElement->GetOwnerDoc();
  IMPL_MUTATION_NOTIFICATION(AttributeChanged, aElement,
                             (doc, aElement, aNameSpaceID, aAttribute,
                              aModType));
}

void
nsNodeUtils::ContentAppended(nsIContent* aContainer,
                             nsIContent* aFirstNewContent,
                             PRInt32 aNewIndexInContainer)
{
  nsIDocument* doc = aContainer->GetOwnerDoc();

  IMPL_MUTATION_NOTIFICATION(ContentAppended, aContainer,
                             (doc, aContainer, aFirstNewContent,
                              aNewIndexInContainer));
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
                            PRInt32 aIndexInContainer,
                            nsIContent* aPreviousSibling)
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
                             (document, container, aChild, aIndexInContainer,
                              aPreviousSibling));
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
    static_cast<nsIDocument*>(aNode)->DeleteAllProperties();
  }
  else {
    if (aNode->HasProperties()) {
      // Strong reference to the document so that deleting properties can't
      // delete the document.
      nsCOMPtr<nsIDocument> document = aNode->GetOwnerDoc();
      if (document) {
        document->DeleteAllPropertiesFor(aNode);
      }
    }

    // I wonder whether it's faster to do the HasFlag check first....
    if (aNode->IsNodeOfType(nsINode::eHTML_FORM_CONTROL) &&
        aNode->HasFlag(ADDED_TO_FORM)) {
      // Tell the form (if any) this node is going away.  Don't
      // notify, since we're being destroyed in any case.
      static_cast<nsGenericHTMLFormElement*>(aNode)->ClearForm(PR_TRUE,
                                                               PR_FALSE);
    }
  }
  aNode->UnsetFlags(NODE_HAS_PROPERTIES);

  if (aNode->HasFlag(NODE_HAS_LISTENERMANAGER)) {
#ifdef DEBUG
    if (nsContentUtils::IsInitialized()) {
      nsIEventListenerManager* manager =
        nsContentUtils::GetListenerManager(aNode, PR_FALSE);
      if (!manager) {
        NS_ERROR("Huh, our bit says we have a listener manager list, "
                 "but there's nothing in the hash!?!!");
      }
    }
#endif

    nsContentUtils::RemoveListenerManager(aNode);
    aNode->UnsetFlags(NODE_HAS_LISTENERMANAGER);
  }

  if (aNode->IsElement()) {
    nsIDocument* ownerDoc = aNode->GetOwnerDoc();
    Element* elem = aNode->AsElement();
    if (ownerDoc) {
      ownerDoc->ClearBoxObjectFor(elem);
    }
    
    NS_ASSERTION(aNode->HasFlag(NODE_FORCE_XBL_BINDINGS) ||
                 !ownerDoc ||
                 !ownerDoc->BindingManager() ||
                 !ownerDoc->BindingManager()->GetBinding(elem),
                 "Non-forced node has binding on destruction");

    // if NODE_FORCE_XBL_BINDINGS is set, the node might still have a binding
    // attached
    if (aNode->HasFlag(NODE_FORCE_XBL_BINDINGS) &&
        ownerDoc && ownerDoc->BindingManager()) {
      ownerDoc->BindingManager()->RemovedFromDocument(elem, ownerDoc);
    }
  }

  nsContentUtils::ReleaseWrapper(aNode, aNode);

  delete aNode;
}

struct NS_STACK_CLASS nsHandlerData
{
  PRUint16 mOperation;
  nsCOMPtr<nsIDOMNode> mSource;
  nsCOMPtr<nsIDOMNode> mDest;
  nsCxPusher mPusher;
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

  if (!handlerData->mPusher.RePush(node)) {
    return;
  }
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

  nsPropertyTable *table = aOwnerDocument->PropertyTable(DOM_USER_DATA_HANDLER);

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

    table->Enumerate(nodeWithProperties, CallHandler, &handlerData);
  }

  return NS_OK;
}

static void
NoteUserData(void *aObject, nsIAtom *aKey, void *aXPCOMChild, void *aData)
{
  nsCycleCollectionTraversalCallback* cb =
    static_cast<nsCycleCollectionTraversalCallback*>(aData);
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb, "[user data (or handler)]");
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

  ownerDoc->PropertyTable(DOM_USER_DATA)->Enumerate(aNode, NoteUserData, &aCb);
  ownerDoc->PropertyTable(DOM_USER_DATA_HANDLER)->Enumerate(aNode, NoteUserData, &aCb);
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

/* static */
nsresult
nsNodeUtils::CloneAndAdopt(nsINode *aNode, PRBool aClone, PRBool aDeep,
                           nsNodeInfoManager *aNewNodeInfoManager,
                           JSContext *aCx, JSObject *aOldScope,
                           JSObject *aNewScope,
                           nsCOMArray<nsINode> &aNodesWithProperties,
                           nsINode *aParent, nsINode **aResult)
{
  NS_PRECONDITION((!aClone && aNewNodeInfoManager) || !aCx,
                  "If cloning or not getting a new nodeinfo we shouldn't "
                  "rewrap");
  NS_PRECONDITION(!aCx || (aOldScope && aNewScope), "Must have scopes");
  NS_PRECONDITION(!aParent || aNode->IsNodeOfType(nsINode::eCONTENT),
                  "Can't insert document or attribute nodes into a parent");

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

    // Don't allow importing/adopting nodes from non-privileged "scriptable"
    // documents to "non-scriptable" documents.
    nsIDocument* newDoc = nodeInfoManager->GetDocument();
    NS_ENSURE_STATE(newDoc);
    PRBool hasHadScriptHandlingObject = PR_FALSE;
    if (!newDoc->GetScriptHandlingObject(hasHadScriptHandlingObject) &&
        !hasHadScriptHandlingObject) {
      nsIDocument* currentDoc = aNode->GetOwnerDoc();
      NS_ENSURE_STATE(currentDoc &&
                      (nsContentUtils::IsChromeDoc(currentDoc) ||
                       (!currentDoc->GetScriptHandlingObject(hasHadScriptHandlingObject) &&
                        !hasHadScriptHandlingObject)));
    }

    newNodeInfo = nodeInfoManager->GetNodeInfo(nodeInfo->NameAtom(),
                                               nodeInfo->GetPrefixAtom(),
                                               nodeInfo->NamespaceID());
    NS_ENSURE_TRUE(newNodeInfo, NS_ERROR_OUT_OF_MEMORY);

    nodeInfo = newNodeInfo;
  }

  nsGenericElement *elem = aNode->IsElement() ?
                           static_cast<nsGenericElement*>(aNode) :
                           nsnull;

  nsCOMPtr<nsINode> clone;
  PRBool isDeepDocumentClone = PR_FALSE;
  if (aClone) {
    rv = aNode->Clone(nodeInfo, getter_AddRefs(clone));
    NS_ENSURE_SUCCESS(rv, rv);

    if (aParent) {
      // If we're cloning we need to insert the cloned children into the cloned
      // parent.
      rv = aParent->AppendChildTo(static_cast<nsIContent*>(clone.get()),
                                  PR_FALSE);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else if (aDeep && clone->IsNodeOfType(nsINode::eDOCUMENT)) {
      isDeepDocumentClone = PR_TRUE;
      // After cloning the document itself, we want to clone the children into
      // the cloned document (somewhat like cloning and importing them into the
      // cloned document).
      nodeInfoManager = clone->mNodeInfo->NodeInfoManager();
    }
  }
  else if (nodeInfoManager) {
    // FIXME Bug 601803 Need to support adopting a node cross-compartment
    if (aCx && aOldScope->compartment() != aNewScope->compartment()) {
      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }

    nsIDocument* oldDoc = aNode->GetOwnerDoc();
    PRBool wasRegistered = PR_FALSE;
    if (oldDoc && aNode->IsElement()) {
      Element* element = aNode->AsElement();
      oldDoc->ClearBoxObjectFor(element);
      wasRegistered = oldDoc->UnregisterFreezableElement(element);
    }

    aNode->mNodeInfo.swap(newNodeInfo);
    if (elem) {
      elem->NodeInfoChanged(newNodeInfo);
    }

    nsIDocument* newDoc = aNode->GetOwnerDoc();
    if (newDoc) {
      // XXX what if oldDoc is null, we don't know if this should be
      // registered or not! Can that really happen?
      if (wasRegistered) {
        newDoc->RegisterFreezableElement(aNode->AsElement());
      }

      nsPIDOMWindow* window = newDoc->GetInnerWindow();
      if (window) {
        nsIEventListenerManager* elm = aNode->GetListenerManager(PR_FALSE);
        if (elm) {
          window->SetMutationListeners(elm->MutationListenerBits());
          if (elm->MayHavePaintEventListener()) {
            window->SetHasPaintEventListeners();
          }
#ifdef MOZ_MEDIA
          if (elm->MayHaveAudioAvailableEventListener()) {
            window->SetHasAudioAvailableEventListeners();
          }
#endif
        }
      }
    }

#ifdef MOZ_MEDIA
    if (wasRegistered && oldDoc != newDoc) {
      nsCOMPtr<nsIDOMHTMLMediaElement> domMediaElem(do_QueryInterface(aNode));
      if (domMediaElem) {
        nsHTMLMediaElement* mediaElem = static_cast<nsHTMLMediaElement*>(aNode);
        mediaElem->NotifyOwnerDocumentActivityChanged();
      }
    }
#endif

    // nsImageLoadingContent needs to know when its document changes
    if (oldDoc != newDoc) {
      nsCOMPtr<nsIImageLoadingContent> imageContent(do_QueryInterface(aNode));
      if (imageContent)
        imageContent->NotifyOwnerDocumentChanged(oldDoc);
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

  // XXX If there are any attribute nodes on this element with UserDataHandlers
  // we should technically adopt/clone/import such attribute nodes and notify
  // those handlers. However we currently don't have code to do so without
  // also notifying when it's not safe so we're not doing that at this time.

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
      nsCOMPtr<nsINode> child;
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
  if (aClone && !aParent && aNode->IsElement() &&
      aNode->AsElement()->IsXUL()) {
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

  clone.forget(aResult);

  return NS_OK;
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
    document->PropertyTable(DOM_USER_DATA)->DeleteAllPropertiesFor(aNode);
    document->PropertyTable(DOM_USER_DATA_HANDLER)->DeleteAllPropertiesFor(aNode);
  }
}
