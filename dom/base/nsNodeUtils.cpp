/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNodeUtils.h"
#include "nsContentUtils.h"
#include "nsCSSPseudoElements.h"
#include "nsINode.h"
#include "nsIContent.h"
#include "mozilla/dom/Element.h"
#include "nsIMutationObserver.h"
#include "nsIDocument.h"
#include "mozilla/EventListenerManager.h"
#include "nsIXPConnect.h"
#include "PLDHashTable.h"
#include "nsIDOMAttr.h"
#include "nsCOMArray.h"
#include "nsPIDOMWindow.h"
#include "nsDocument.h"
#ifdef MOZ_XUL
#include "nsXULElement.h"
#endif
#include "nsBindingManager.h"
#include "nsGenericHTMLElement.h"
#include "mozilla/AnimationTarget.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/Animation.h"
#include "mozilla/dom/HTMLImageElement.h"
#include "mozilla/dom/HTMLMediaElement.h"
#include "mozilla/dom/KeyframeEffect.h"
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

#define IMPL_ANIMATION_NOTIFICATION(func_, content_, params_)     \
  PR_BEGIN_MACRO                                                  \
  bool needsEnterLeave = doc->MayHaveDOMMutationObservers();      \
  if (needsEnterLeave) {                                          \
    nsDOMMutationObserver::EnterMutationHandling();               \
  }                                                               \
  nsINode* node = content_;                                       \
  do {                                                            \
    nsINode::nsSlots* slots = node->GetExistingSlots();           \
    if (slots && !slots->mMutationObservers.IsEmpty()) {          \
      /* No need to explicitly notify the first observer first    \
         since that'll happen anyway. */                          \
      NS_OBSERVER_ARRAY_NOTIFY_OBSERVERS_WITH_QI(                 \
        slots->mMutationObservers, nsIMutationObserver,           \
        nsIAnimationObserver, func_, params_);                    \
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
                                 int32_t aModType,
                                 const nsAttrValue* aNewValue)
{
  nsIDocument* doc = aElement->OwnerDoc();
  IMPL_MUTATION_NOTIFICATION(AttributeWillChange, aElement,
                             (doc, aElement, aNameSpaceID, aAttribute,
                              aModType, aNewValue));
}

void
nsNodeUtils::AttributeChanged(Element* aElement,
                              int32_t aNameSpaceID,
                              nsIAtom* aAttribute,
                              int32_t aModType,
                              const nsAttrValue* aOldValue)
{
  nsIDocument* doc = aElement->OwnerDoc();
  IMPL_MUTATION_NOTIFICATION(AttributeChanged, aElement,
                             (doc, aElement, aNameSpaceID, aAttribute,
                              aModType, aOldValue));
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
nsNodeUtils::NativeAnonymousChildListChange(nsIContent* aContent,
                                            bool aIsRemove)
{
  nsIDocument* doc = aContent->OwnerDoc();
  IMPL_MUTATION_NOTIFICATION(NativeAnonymousChildListChange, aContent,
                            (doc, aContent, aIsRemove));
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

Maybe<NonOwningAnimationTarget>
nsNodeUtils::GetTargetForAnimation(const Animation* aAnimation)
{
  KeyframeEffectReadOnly* effect = aAnimation->GetEffect();
  return effect ? effect->GetTarget() : Nothing();
}

void
nsNodeUtils::AnimationMutated(Animation* aAnimation,
                              AnimationMutationType aMutatedType)
{
  Maybe<NonOwningAnimationTarget> target = GetTargetForAnimation(aAnimation);
  if (!target) {
    return;
  }

  // A pseudo element and its parent element use the same owner doc.
  nsIDocument* doc = target->mElement->OwnerDoc();
  if (doc->MayHaveAnimationObservers()) {
    // we use the its parent element as the subject in DOM Mutation Observer.
    Element* elem = target->mElement;
    switch (aMutatedType) {
      case AnimationMutationType::Added:
        IMPL_ANIMATION_NOTIFICATION(AnimationAdded, elem, (aAnimation));
        break;
      case AnimationMutationType::Changed:
        IMPL_ANIMATION_NOTIFICATION(AnimationChanged, elem, (aAnimation));
        break;
      case AnimationMutationType::Removed:
        IMPL_ANIMATION_NOTIFICATION(AnimationRemoved, elem, (aAnimation));
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("unexpected mutation type");
    }
  }
}

void
nsNodeUtils::AnimationAdded(Animation* aAnimation)
{
  AnimationMutated(aAnimation, AnimationMutationType::Added);
}

void
nsNodeUtils::AnimationChanged(Animation* aAnimation)
{
  AnimationMutated(aAnimation, AnimationMutationType::Changed);
}

void
nsNodeUtils::AnimationRemoved(Animation* aAnimation)
{
  AnimationMutated(aAnimation, AnimationMutationType::Removed);
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

    if (aNode->IsHTMLElement(nsGkAtoms::img) &&
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
      ownerDoc->BindingManager()->RemovedFromDocument(elem, ownerDoc,
                                                      nsBindingManager::eRunDtor);
    }
  }

  aNode->ReleaseWrapper(aNode);

  FragmentOrElement::RemoveBlackMarkedNode(aNode);
}

static void
NoteUserData(void *aObject, nsIAtom *aKey, void *aXPCOMChild, void *aData)
{
  nsCycleCollectionTraversalCallback* cb =
    static_cast<nsCycleCollectionTraversalCallback*>(aData);
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb, "[user data]");
  cb->NoteXPCOMChild(static_cast<nsISupports*>(aXPCOMChild));
}

/* static */
void
nsNodeUtils::TraverseUserData(nsINode* aNode,
                              nsCycleCollectionTraversalCallback &aCb)
{
  nsIDocument* ownerDoc = aNode->OwnerDoc();
  ownerDoc->PropertyTable(DOM_USER_DATA)->Enumerate(aNode, NoteUserData, &aCb);
}

/* static */
nsresult
nsNodeUtils::CloneNodeImpl(nsINode *aNode, bool aDeep, nsINode **aResult)
{
  *aResult = nullptr;

  nsCOMPtr<nsINode> newNode;
  nsCOMArray<nsINode> nodesWithProperties;
  nsresult rv = Clone(aNode, aDeep, nullptr, nodesWithProperties,
                      getter_AddRefs(newNode));
  NS_ENSURE_SUCCESS(rv, rv);

  newNode.forget(aResult);
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

  nsAutoScriptBlocker scriptBlocker;
  nsresult rv;

  nsNodeInfoManager *nodeInfoManager = aNewNodeInfoManager;

  // aNode.
  NodeInfo *nodeInfo = aNode->mNodeInfo;
  RefPtr<NodeInfo> newNodeInfo;
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

    if (clone->IsElement()) {
      // The cloned node may be a custom element that may require
      // enqueing created callback and prototype swizzling.
      Element* elem = clone->AsElement();
      if (nsContentUtils::IsCustomElementName(nodeInfo->NameAtom())) {
        elem->OwnerDoc()->SetupCustomElement(elem, nodeInfo->NamespaceID());
      } else {
        // Check if node may be custom element by type extension.
        // ex. <button is="x-button">
        nsAutoString extension;
        if (elem->GetAttr(kNameSpaceID_None, nsGkAtoms::is, extension) &&
            !extension.IsEmpty()) {
          elem->OwnerDoc()->SetupCustomElement(elem, nodeInfo->NamespaceID(),
                                               &extension);
        }
      }
    }

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
      wasRegistered = oldDoc->UnregisterActivityObserver(element);
    }

    aNode->mNodeInfo.swap(newNodeInfo);
    if (elem) {
      elem->NodeInfoChanged();
    }

    nsIDocument* newDoc = aNode->OwnerDoc();
    if (newDoc) {
      // XXX what if oldDoc is null, we don't know if this should be
      // registered or not! Can that really happen?
      if (wasRegistered) {
        newDoc->RegisterActivityObserver(aNode->AsElement());
      }

      if (nsPIDOMWindowInner* window = newDoc->GetInnerWindow()) {
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

    if (oldDoc != newDoc && oldDoc->MayHaveAnimationObservers()) {
      newDoc->SetMayHaveAnimationObservers();
    }

    if (elem) {
      elem->RecompileScriptEventListeners();
    }

    if (aReparentScope) {
      AutoJSContext cx;
      JS::Rooted<JSObject*> wrapper(cx);
      if ((wrapper = aNode->GetWrapper())) {
        MOZ_ASSERT(IsDOMObject(wrapper));
        JSAutoCompartment ac(cx, wrapper);
        rv = ReparentWrapper(cx, wrapper);
        if (NS_FAILED(rv)) {
          aNode->mNodeInfo.swap(newNodeInfo);

          return rv;
        }
      }
    }
  }

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
  if (aClone && !aParent && aNode->IsXULElement()) {
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
}

bool
nsNodeUtils::IsTemplateElement(const nsINode *aNode)
{
  return aNode->IsHTMLElement(nsGkAtoms::_template);
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

