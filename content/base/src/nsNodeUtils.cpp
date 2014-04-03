/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNodeUtils.h"
#include "nsContentUtils.h"
#include "nsCxPusher.h"
#include "nsINode.h"
#include "nsIContent.h"
#include "mozilla/dom/Element.h"
#include "nsIMutationObserver.h"
#include "nsIDocument.h"
#include "nsIDOMUserDataHandler.h"
#include "mozilla/EventListenerManager.h"
#include "nsIXPConnect.h"
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
#include "mozilla/dom/HTMLImageElement.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "nsWrapperCacheInlines.h"
#include "nsObjectLoadingContent.h"
#include "nsDOMMutationObserver.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/HTMLTemplateElement.h"
#include "mozilla/dom/ShadowRoot.h"

using namespace mozilla;
using namespace mozilla::dom;
using mozilla::AutoJSContext;

// This macro expects the ownerDocument of content_ to be in scope as
// |nsIDocument* doc|
#define IMPL_MUTATION_NOTIFICATION(func_, content_, params_)      \
  PR_BEGIN_MACRO                                                  \
  bool needsEnterLeave = doc->MayHaveDOMMutationObservers();      \
  if (needsEnterLeave) {                                          \
    nsDOMMutationObserver::EnterMutationHandling();               \
  }                                                               \
  nsINode* node = content_;                                       \
  NS_ASSERTION(node->OwnerDoc() == doc, "Bogus document");        \
  if (doc) {                                                      \
    doc->BindingManager()->func_ params_;                         \
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
    ShadowRoot* shadow = ShadowRoot::FromNode(node);              \
    if (shadow) {                                                 \
      node = shadow->GetPoolHost();                               \
    } else {                                                      \
      node = node->GetParentNode();                               \
    }                                                             \
  } while (node);                                                 \
  if (needsEnterLeave) {                                          \
    nsDOMMutationObserver::LeaveMutationHandling();               \
  }                                                               \
  PR_END_MACRO

void
nsNodeUtils::CharacterDataWillChange(nsIContent* aContent,
                                     CharacterDataChangeInfo* aInfo)
{
  nsIDocument* doc = aContent->OwnerDoc();
  IMPL_MUTATION_NOTIFICATION(CharacterDataWillChange, aContent,
                             (doc, aContent, aInfo));
}

void
nsNodeUtils::CharacterDataChanged(nsIContent* aContent,
                                  CharacterDataChangeInfo* aInfo)
{
  nsIDocument* doc = aContent->OwnerDoc();
  IMPL_MUTATION_NOTIFICATION(CharacterDataChanged, aContent,
                             (doc, aContent, aInfo));
}

void
nsNodeUtils::AttributeWillChange(Element* aElement,
                                 int32_t aNameSpaceID,
                                 nsIAtom* aAttribute,
                                 int32_t aModType)
{
  nsIDocument* doc = aElement->OwnerDoc();
  IMPL_MUTATION_NOTIFICATION(AttributeWillChange, aElement,
                             (doc, aElement, aNameSpaceID, aAttribute,
                              aModType));
}

void
nsNodeUtils::AttributeChanged(Element* aElement,
                              int32_t aNameSpaceID,
                              nsIAtom* aAttribute,
                              int32_t aModType)
{
  nsIDocument* doc = aElement->OwnerDoc();
  IMPL_MUTATION_NOTIFICATION(AttributeChanged, aElement,
                             (doc, aElement, aNameSpaceID, aAttribute,
                              aModType));
}

void
nsNodeUtils::AttributeSetToCurrentValue(Element* aElement,
                                        int32_t aNameSpaceID,
                                        nsIAtom* aAttribute)
{
  nsIDocument* doc = aElement->OwnerDoc();
  IMPL_MUTATION_NOTIFICATION(AttributeSetToCurrentValue, aElement,
                             (doc, aElement, aNameSpaceID, aAttribute));
}

void
nsNodeUtils::ContentAppended(nsIContent* aContainer,
                             nsIContent* aFirstNewContent,
                             int32_t aNewIndexInContainer)
{
  nsIDocument* doc = aContainer->OwnerDoc();

  IMPL_MUTATION_NOTIFICATION(ContentAppended, aContainer,
                             (doc, aContainer, aFirstNewContent,
                              aNewIndexInContainer));
}

void
nsNodeUtils::ContentInserted(nsINode* aContainer,
                             nsIContent* aChild,
                             int32_t aIndexInContainer)
{
  NS_PRECONDITION(aContainer->IsNodeOfType(nsINode::eCONTENT) ||
                  aContainer->IsNodeOfType(nsINode::eDOCUMENT),
                  "container must be an nsIContent or an nsIDocument");
  nsIContent* container;
  nsIDocument* doc = aContainer->OwnerDoc();
  nsIDocument* document;
  if (aContainer->IsNodeOfType(nsINode::eCONTENT)) {
    container = static_cast<nsIContent*>(aContainer);
    document = doc;
  }
  else {
    container = nullptr;
    document = static_cast<nsIDocument*>(aContainer);
  }

  IMPL_MUTATION_NOTIFICATION(ContentInserted, aContainer,
                             (document, container, aChild, aIndexInContainer));
}

void
nsNodeUtils::ContentRemoved(nsINode* aContainer,
                            nsIContent* aChild,
                            int32_t aIndexInContainer,
                            nsIContent* aPreviousSibling)
{
  NS_PRECONDITION(aContainer->IsNodeOfType(nsINode::eCONTENT) ||
                  aContainer->IsNodeOfType(nsINode::eDOCUMENT),
                  "container must be an nsIContent or an nsIDocument");
  nsIContent* container;
  nsIDocument* doc = aContainer->OwnerDoc();
  nsIDocument* document;
  if (aContainer->IsNodeOfType(nsINode::eCONTENT)) {
    container = static_cast<nsIContent*>(aContainer);
    document = doc;
  }
  else {
    container = nullptr;
    document = static_cast<nsIDocument*>(aContainer);
  }

  IMPL_MUTATION_NOTIFICATION(ContentRemoved, aContainer,
                             (document, container, aChild, aIndexInContainer,
                              aPreviousSibling));
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

    delete slots;
    aNode->mSlots = nullptr;
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
      nsCOMPtr<nsIDocument> document = aNode->OwnerDoc();
      document->DeleteAllPropertiesFor(aNode);
    }

    // I wonder whether it's faster to do the HasFlag check first....
    if (aNode->IsNodeOfType(nsINode::eHTML_FORM_CONTROL) &&
        aNode->HasFlag(ADDED_TO_FORM)) {
      // Tell the form (if any) this node is going away.  Don't
      // notify, since we're being destroyed in any case.
      static_cast<nsGenericHTMLFormElement*>(aNode)->ClearForm(true);
    }

    if (aNode->IsElement() && aNode->AsElement()->IsHTML(nsGkAtoms::img) &&
        aNode->HasFlag(ADDED_TO_FORM)) {
      HTMLImageElement* imageElem = static_cast<HTMLImageElement*>(aNode);
      imageElem->ClearForm(true);
    }
  }
  aNode->UnsetFlags(NODE_HAS_PROPERTIES);

  if (aNode->NodeType() != nsIDOMNode::DOCUMENT_NODE &&
      aNode->HasFlag(NODE_HAS_LISTENERMANAGER)) {
#ifdef DEBUG
    if (nsContentUtils::IsInitialized()) {
      EventListenerManager* manager =
        nsContentUtils::GetExistingListenerManagerForNode(aNode);
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
    nsIDocument* ownerDoc = aNode->OwnerDoc();
    Element* elem = aNode->AsElement();
    ownerDoc->ClearBoxObjectFor(elem);

    NS_ASSERTION(aNode->HasFlag(NODE_FORCE_XBL_BINDINGS) ||
                 !elem->GetXBLBinding(),
                 "Non-forced node has binding on destruction");

    // if NODE_FORCE_XBL_BINDINGS is set, the node might still have a binding
    // attached
    if (aNode->HasFlag(NODE_FORCE_XBL_BINDINGS) &&
        ownerDoc->BindingManager()) {
      ownerDoc->BindingManager()->RemovedFromDocument(elem, ownerDoc);
    }
  }

  aNode->ReleaseWrapper(aNode);

  FragmentOrElement::RemoveBlackMarkedNode(aNode);
}

struct MOZ_STACK_CLASS nsHandlerData
{
  uint16_t mOperation;
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
                                  uint16_t aOperation, bool aCloned)
{
  NS_PRECONDITION(!aCloned || (aNodesWithProperties.Count() % 2 == 0),
                  "Expected aNodesWithProperties to contain original and "
                  "cloned nodes.");

  if (!nsContentUtils::IsSafeToRunScript()) {
    if (nsContentUtils::IsChromeDoc(aOwnerDocument)) {
      NS_WARNING("Fix the caller! Userdata callback disabled.");
    } else {
      NS_ERROR("This is unsafe! Fix the caller! Userdata callback disabled.");
    }

    return NS_OK;
  }

  nsPropertyTable *table = aOwnerDocument->PropertyTable(DOM_USER_DATA_HANDLER);

  // Keep the document alive, just in case one of the handlers causes it to go
  // away.
  nsCOMPtr<nsIDocument> ownerDoc = aOwnerDocument;

  nsHandlerData handlerData;
  handlerData.mOperation = aOperation;

  uint32_t i, count = aNodesWithProperties.Count();
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
  nsIDocument* ownerDoc = aNode->OwnerDoc();
  ownerDoc->PropertyTable(DOM_USER_DATA)->Enumerate(aNode, NoteUserData, &aCb);
  ownerDoc->PropertyTable(DOM_USER_DATA_HANDLER)->Enumerate(aNode, NoteUserData, &aCb);
}

/* static */
nsresult
nsNodeUtils::CloneNodeImpl(nsINode *aNode, bool aDeep,
                           bool aCallUserDataHandlers,
                           nsINode **aResult)
{
  *aResult = nullptr;

  nsCOMPtr<nsINode> newNode;
  nsCOMArray<nsINode> nodesWithProperties;
  nsresult rv = Clone(aNode, aDeep, nullptr, nodesWithProperties,
                      getter_AddRefs(newNode));
  NS_ENSURE_SUCCESS(rv, rv);

  if (aCallUserDataHandlers) {
    rv = CallUserDataHandlers(nodesWithProperties, aNode->OwnerDoc(),
                              nsIDOMUserDataHandler::NODE_CLONED, true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  newNode.swap(*aResult);

  return NS_OK;
}

/* static */
nsresult
nsNodeUtils::CloneAndAdopt(nsINode *aNode, bool aClone, bool aDeep,
                           nsNodeInfoManager *aNewNodeInfoManager,
                           JS::Handle<JSObject*> aReparentScope,
                           nsCOMArray<nsINode> &aNodesWithProperties,
                           nsINode *aParent, nsINode **aResult)
{
  NS_PRECONDITION((!aClone && aNewNodeInfoManager) || !aReparentScope,
                  "If cloning or not getting a new nodeinfo we shouldn't "
                  "rewrap");
  NS_PRECONDITION(!aParent || aNode->IsNodeOfType(nsINode::eCONTENT),
                  "Can't insert document or attribute nodes into a parent");

  *aResult = nullptr;

  // First deal with aNode and walk its attributes (and their children). Then,
  // if aDeep is true, deal with aNode's children (and recurse into their
  // attributes and children).

  AutoJSContext cx;
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
    bool hasHadScriptHandlingObject = false;
    if (!newDoc->GetScriptHandlingObject(hasHadScriptHandlingObject) &&
        !hasHadScriptHandlingObject) {
      nsIDocument* currentDoc = aNode->OwnerDoc();
      NS_ENSURE_STATE((nsContentUtils::IsChromeDoc(currentDoc) ||
                       (!currentDoc->GetScriptHandlingObject(hasHadScriptHandlingObject) &&
                        !hasHadScriptHandlingObject)));
    }

    newNodeInfo = nodeInfoManager->GetNodeInfo(nodeInfo->NameAtom(),
                                               nodeInfo->GetPrefixAtom(),
                                               nodeInfo->NamespaceID(),
                                               nodeInfo->NodeType(),
                                               nodeInfo->GetExtraName());

    nodeInfo = newNodeInfo;
  }

  Element *elem = aNode->IsElement() ? aNode->AsElement() : nullptr;

  nsCOMPtr<nsINode> clone;
  if (aClone) {
    rv = aNode->Clone(nodeInfo, getter_AddRefs(clone));
    NS_ENSURE_SUCCESS(rv, rv);

    if (aParent) {
      // If we're cloning we need to insert the cloned children into the cloned
      // parent.
      rv = aParent->AppendChildTo(static_cast<nsIContent*>(clone.get()),
                                  false);
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
    nsIDocument* oldDoc = aNode->OwnerDoc();
    bool wasRegistered = false;
    if (aNode->IsElement()) {
      Element* element = aNode->AsElement();
      oldDoc->ClearBoxObjectFor(element);
      wasRegistered = oldDoc->UnregisterFreezableElement(element);
    }

    aNode->mNodeInfo.swap(newNodeInfo);
    if (elem) {
      elem->NodeInfoChanged(newNodeInfo);
    }

    nsIDocument* newDoc = aNode->OwnerDoc();
    if (newDoc) {
      // XXX what if oldDoc is null, we don't know if this should be
      // registered or not! Can that really happen?
      if (wasRegistered) {
        newDoc->RegisterFreezableElement(aNode->AsElement());
      }

      nsPIDOMWindow* window = newDoc->GetInnerWindow();
      if (window) {
        EventListenerManager* elm = aNode->GetExistingListenerManager();
        if (elm) {
          window->SetMutationListeners(elm->MutationListenerBits());
          if (elm->MayHavePaintEventListener()) {
            window->SetHasPaintEventListeners();
          }
          if (elm->MayHaveTouchEventListener()) {
            window->SetHasTouchEventListeners();
          }
          if (elm->MayHaveMouseEnterLeaveEventListener()) {
            window->SetHasMouseEnterLeaveEventListeners();
          }
          if (elm->MayHavePointerEnterLeaveEventListener()) {
            window->SetHasPointerEnterLeaveEventListeners();
          }
        }
      }
    }

    if (wasRegistered && oldDoc != newDoc) {
      nsCOMPtr<nsIDOMHTMLMediaElement> domMediaElem(do_QueryInterface(aNode));
      if (domMediaElem) {
        HTMLMediaElement* mediaElem = static_cast<HTMLMediaElement*>(aNode);
        mediaElem->NotifyOwnerDocumentActivityChanged();
      }
      nsCOMPtr<nsIObjectLoadingContent> objectLoadingContent(do_QueryInterface(aNode));
      if (objectLoadingContent) {
        nsObjectLoadingContent* olc = static_cast<nsObjectLoadingContent*>(objectLoadingContent.get());
        olc->NotifyOwnerDocumentActivityChanged();
      }
    }

    if (oldDoc != newDoc && oldDoc->MayHaveDOMMutationObservers()) {
      newDoc->SetMayHaveDOMMutationObservers();
    }

    if (elem) {
      elem->RecompileScriptEventListeners();
    }

    if (aReparentScope) {
      JS::Rooted<JSObject*> wrapper(cx);
      if ((wrapper = aNode->GetWrapper())) {
        if (IsDOMObject(wrapper)) {
          rv = ReparentWrapper(cx, wrapper);
        } else {
          nsIXPConnect *xpc = nsContentUtils::XPConnect();
          if (xpc) {
            rv = xpc->ReparentWrappedNativeIfFound(cx, wrapper, aReparentScope, aNode);
          } else {
            rv = NS_ERROR_FAILURE;
          }
        }
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

  if (aDeep && (!aClone || !aNode->IsNodeOfType(nsINode::eATTRIBUTE))) {
    // aNode's children.
    for (nsIContent* cloneChild = aNode->GetFirstChild();
         cloneChild;
         cloneChild = cloneChild->GetNextSibling()) {
      nsCOMPtr<nsINode> child;
      rv = CloneAndAdopt(cloneChild, aClone, true, nodeInfoManager,
                         aReparentScope, aNodesWithProperties, clone,
                         getter_AddRefs(child));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // Cloning template element.
  if (aDeep && aClone && IsTemplateElement(aNode)) {
    DocumentFragment* origContent =
      static_cast<HTMLTemplateElement*>(aNode)->Content();
    DocumentFragment* cloneContent =
      static_cast<HTMLTemplateElement*>(clone.get())->Content();

    // Clone the children into the clone's template content owner
    // document's nodeinfo manager.
    nsNodeInfoManager* ownerNodeInfoManager =
      cloneContent->mNodeInfo->NodeInfoManager();

    for (nsIContent* cloneChild = origContent->GetFirstChild();
         cloneChild;
         cloneChild = cloneChild->GetNextSibling()) {
      nsCOMPtr<nsINode> child;
      rv = CloneAndAdopt(cloneChild, aClone, aDeep, ownerNodeInfoManager,
                         aReparentScope, aNodesWithProperties, cloneContent,
                         getter_AddRefs(child));
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
    if (!aNode->OwnerDoc()->IsLoadedAsInteractiveData()) {
      clone->SetFlags(NODE_FORCE_XBL_BINDINGS);
    }
  }
#endif

  if (aNode->HasProperties()) {
    bool ok = aNodesWithProperties.AppendObject(aNode);
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
  nsCOMPtr<nsIDocument> document = aNode->OwnerDoc();
  document->PropertyTable(DOM_USER_DATA)->DeleteAllPropertiesFor(aNode);
  document->PropertyTable(DOM_USER_DATA_HANDLER)->DeleteAllPropertiesFor(aNode);
}

bool
nsNodeUtils::IsTemplateElement(const nsINode *aNode)
{
  return aNode->IsElement() && aNode->AsElement()->IsHTML(nsGkAtoms::_template);
}

nsIContent*
nsNodeUtils::GetFirstChildOfTemplateOrNode(nsINode* aNode)
{
  if (nsNodeUtils::IsTemplateElement(aNode)) {
    DocumentFragment* frag =
      static_cast<HTMLTemplateElement*>(aNode)->Content();
    return frag->GetFirstChild();
  }

  return aNode->GetFirstChild();
}

