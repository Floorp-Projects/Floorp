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
#include "nsIContentInlines.h"
#include "mozilla/dom/Element.h"
#include "nsIMutationObserver.h"
#include "nsIDocument.h"
#include "mozilla/EventListenerManager.h"
#include "nsIXPConnect.h"
#include "PLDHashTable.h"
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
#include "mozilla/ErrorResult.h"
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

enum class IsRemoveNotification
{
  Yes,
  No,
};

#ifdef DEBUG
#define COMPOSED_DOC_DECL \
  const bool wasInComposedDoc = !!node->GetComposedDoc();
#else
#define COMPOSED_DOC_DECL
#endif

// This macro expects the ownerDocument of content_ to be in scope as
// |nsIDocument* doc|
#define IMPL_MUTATION_NOTIFICATION(func_, content_, params_, remove_)       \
  PR_BEGIN_MACRO                                                            \
  bool needsEnterLeave = doc->MayHaveDOMMutationObservers();                \
  if (needsEnterLeave) {                                                    \
    nsDOMMutationObserver::EnterMutationHandling();                         \
  }                                                                         \
  nsINode* node = content_;                                                 \
  COMPOSED_DOC_DECL                                                         \
  NS_ASSERTION(node->OwnerDoc() == doc, "Bogus document");                  \
  if (remove_ == IsRemoveNotification::Yes && node->GetComposedDoc()) {     \
    if (nsIPresShell* shell = doc->GetObservingShell()) {                   \
      shell->func_ params_;                                                 \
    }                                                                       \
  }                                                                         \
  doc->BindingManager()->func_ params_;                                     \
  nsINode* last;                                                            \
  do {                                                                      \
    nsINode::nsSlots* slots = node->GetExistingSlots();                     \
    if (slots && !slots->mMutationObservers.IsEmpty()) {                    \
      NS_OBSERVER_AUTO_ARRAY_NOTIFY_OBSERVERS(                              \
        slots->mMutationObservers, nsIMutationObserver, 1,                  \
        func_, params_);                                                    \
    }                                                                       \
    last = node;                                                            \
    if (ShadowRoot* shadow = ShadowRoot::FromNode(node)) {                  \
      node = shadow->GetHost();                                             \
    } else {                                                                \
      node = node->GetParentNode();                                         \
    }                                                                       \
  } while (node);                                                           \
  /* Whitelist NativeAnonymousChildListChange removal notifications from    \
   * the assertion since it runs from UnbindFromTree, and thus we don't     \
   * reach the document, but doesn't matter. */                             \
  MOZ_ASSERT((last == doc) == wasInComposedDoc ||                           \
             (remove_ == IsRemoveNotification::Yes &&                       \
              !strcmp(#func_, "NativeAnonymousChildListChange")));          \
  if (remove_ == IsRemoveNotification::No && last == doc) {                 \
    if (nsIPresShell* shell = doc->GetObservingShell()) {                   \
      shell->func_ params_;                                                 \
    }                                                                       \
  }                                                                         \
  if (needsEnterLeave) {                                                    \
    nsDOMMutationObserver::LeaveMutationHandling();                         \
  }                                                                         \
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
      NS_OBSERVER_AUTO_ARRAY_NOTIFY_OBSERVERS_WITH_QI(            \
        slots->mMutationObservers, nsIMutationObserver, 1,        \
        nsIAnimationObserver, func_, params_);                    \
    }                                                             \
    if (ShadowRoot* shadow = ShadowRoot::FromNode(node)) {        \
      node = shadow->GetHost();                                   \
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
                                     const CharacterDataChangeInfo& aInfo)
{
  nsIDocument* doc = aContent->OwnerDoc();
  IMPL_MUTATION_NOTIFICATION(CharacterDataWillChange, aContent,
                             (aContent, aInfo), IsRemoveNotification::No);
}

void
nsNodeUtils::CharacterDataChanged(nsIContent* aContent,
                                  const CharacterDataChangeInfo& aInfo)
{
  nsIDocument* doc = aContent->OwnerDoc();
  IMPL_MUTATION_NOTIFICATION(CharacterDataChanged, aContent,
                             (aContent, aInfo), IsRemoveNotification::No);
}

void
nsNodeUtils::AttributeWillChange(Element* aElement,
                                 int32_t aNameSpaceID,
                                 nsAtom* aAttribute,
                                 int32_t aModType,
                                 const nsAttrValue* aNewValue)
{
  nsIDocument* doc = aElement->OwnerDoc();
  IMPL_MUTATION_NOTIFICATION(AttributeWillChange, aElement,
                             (aElement, aNameSpaceID, aAttribute,
                              aModType, aNewValue), IsRemoveNotification::No);
}

void
nsNodeUtils::AttributeChanged(Element* aElement,
                              int32_t aNameSpaceID,
                              nsAtom* aAttribute,
                              int32_t aModType,
                              const nsAttrValue* aOldValue)
{
  nsIDocument* doc = aElement->OwnerDoc();
  IMPL_MUTATION_NOTIFICATION(AttributeChanged, aElement,
                             (aElement, aNameSpaceID, aAttribute,
                              aModType, aOldValue), IsRemoveNotification::No);
}

void
nsNodeUtils::AttributeSetToCurrentValue(Element* aElement,
                                        int32_t aNameSpaceID,
                                        nsAtom* aAttribute)
{
  nsIDocument* doc = aElement->OwnerDoc();
  IMPL_MUTATION_NOTIFICATION(AttributeSetToCurrentValue, aElement,
                             (aElement, aNameSpaceID, aAttribute),
                             IsRemoveNotification::No);
}

void
nsNodeUtils::ContentAppended(nsIContent* aContainer,
                             nsIContent* aFirstNewContent)
{
  nsIDocument* doc = aContainer->OwnerDoc();

  IMPL_MUTATION_NOTIFICATION(ContentAppended, aContainer,
                             (aFirstNewContent),
                             IsRemoveNotification::No);
}

void
nsNodeUtils::NativeAnonymousChildListChange(nsIContent* aContent,
                                            bool aIsRemove)
{
  nsIDocument* doc = aContent->OwnerDoc();
  auto isRemove = aIsRemove
    ? IsRemoveNotification::Yes : IsRemoveNotification::No;
  IMPL_MUTATION_NOTIFICATION(NativeAnonymousChildListChange, aContent,
                            (aContent, aIsRemove),
                            isRemove);
}

void
nsNodeUtils::ContentInserted(nsINode* aContainer,
                             nsIContent* aChild)
{
  MOZ_ASSERT(aContainer->IsContent() || aContainer->IsDocument(),
             "container must be an nsIContent or an nsIDocument");
  nsIDocument* doc = aContainer->OwnerDoc();
  IMPL_MUTATION_NOTIFICATION(ContentInserted, aContainer, (aChild),
                             IsRemoveNotification::No);
}

void
nsNodeUtils::ContentRemoved(nsINode* aContainer,
                            nsIContent* aChild,
                            nsIContent* aPreviousSibling)
{
  MOZ_ASSERT(aContainer->IsContent() || aContainer->IsDocument(),
             "container must be an nsIContent or an nsIDocument");
  nsIDocument* doc = aContainer->OwnerDoc();
  MOZ_ASSERT(aChild->GetParentNode() == aContainer,
             "We expect the parent link to be still around at this point");
  IMPL_MUTATION_NOTIFICATION(ContentRemoved, aContainer,
                             (aChild, aPreviousSibling),
                             IsRemoveNotification::Yes);
}

Maybe<NonOwningAnimationTarget>
nsNodeUtils::GetTargetForAnimation(const Animation* aAnimation)
{
  AnimationEffect* effect = aAnimation->GetEffect();
  if (!effect || !effect->AsKeyframeEffect()) {
    return Nothing();
  }
  return effect->AsKeyframeEffect()->GetTarget();
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
      NS_OBSERVER_AUTO_ARRAY_NOTIFY_OBSERVERS(slots->mMutationObservers,
                                              nsIMutationObserver, 1,
                                              NodeWillBeDestroyed, (aNode));
    }

    delete slots;
    aNode->mSlots = nullptr;
  }

  // Kill properties first since that may run external code, so we want to
  // be in as complete state as possible at that time.
  if (aNode->IsDocument()) {
    // Delete all properties before tearing down the document. Some of the
    // properties are bound to nsINode objects and the destructor functions of
    // the properties may want to use the owner document of the nsINode.
    aNode->AsDocument()->DeleteAllProperties();
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
      static_cast<nsGenericHTMLFormElement*>(aNode)->ClearForm(true, true);
    }

    if (aNode->IsHTMLElement(nsGkAtoms::img) &&
        aNode->HasFlag(ADDED_TO_FORM)) {
      HTMLImageElement* imageElem = static_cast<HTMLImageElement*>(aNode);
      imageElem->ClearForm(true);
    }
  }
  aNode->UnsetFlags(NODE_HAS_PROPERTIES);

  if (aNode->NodeType() != nsINode::DOCUMENT_NODE &&
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

  if (Element* element = Element::FromNode(aNode)) {
    element->OwnerDoc()->ClearBoxObjectFor(element);
    NS_ASSERTION(!element->GetXBLBinding(), "Node has binding on destruction");
  }

  aNode->ReleaseWrapper(aNode);

  FragmentOrElement::RemoveBlackMarkedNode(aNode);
}

/* static */
already_AddRefed<nsINode>
nsNodeUtils::CloneNodeImpl(nsINode *aNode, bool aDeep, ErrorResult& aError)
{
  return Clone(aNode, aDeep, nullptr, nullptr, aError);
}

/* static */
already_AddRefed<nsINode>
nsNodeUtils::CloneAndAdopt(nsINode *aNode, bool aClone, bool aDeep,
                           nsNodeInfoManager *aNewNodeInfoManager,
                           JS::Handle<JSObject*> aReparentScope,
                           nsCOMArray<nsINode> *aNodesWithProperties,
                           nsINode* aParent, ErrorResult& aError)
{
  MOZ_ASSERT((!aClone && aNewNodeInfoManager) || !aReparentScope,
              "If cloning or not getting a new nodeinfo we shouldn't rewrap");
  MOZ_ASSERT(!aParent || aNode->IsContent(),
             "Can't insert document or attribute nodes into a parent");

  // First deal with aNode and walk its attributes (and their children). Then,
  // if aDeep is true, deal with aNode's children (and recurse into their
  // attributes and children).

  nsAutoScriptBlocker scriptBlocker;

  nsNodeInfoManager *nodeInfoManager = aNewNodeInfoManager;

  // aNode.
  NodeInfo *nodeInfo = aNode->mNodeInfo;
  RefPtr<NodeInfo> newNodeInfo;
  if (nodeInfoManager) {

    // Don't allow importing/adopting nodes from non-privileged "scriptable"
    // documents to "non-scriptable" documents.
    nsIDocument* newDoc = nodeInfoManager->GetDocument();
    if (NS_WARN_IF(!newDoc)) {
      aError.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    bool hasHadScriptHandlingObject = false;
    if (!newDoc->GetScriptHandlingObject(hasHadScriptHandlingObject) &&
        !hasHadScriptHandlingObject) {
      nsIDocument* currentDoc = aNode->OwnerDoc();
      if (NS_WARN_IF(!nsContentUtils::IsChromeDoc(currentDoc) &&
                     (currentDoc->GetScriptHandlingObject(hasHadScriptHandlingObject) ||
                      hasHadScriptHandlingObject))) {
        aError.Throw(NS_ERROR_UNEXPECTED);
        return nullptr;
      }
    }

    newNodeInfo = nodeInfoManager->GetNodeInfo(nodeInfo->NameAtom(),
                                               nodeInfo->GetPrefixAtom(),
                                               nodeInfo->NamespaceID(),
                                               nodeInfo->NodeType(),
                                               nodeInfo->GetExtraName());

    nodeInfo = newNodeInfo;
  }

  Element* elem = Element::FromNode(aNode);

  nsCOMPtr<nsINode> clone;
  if (aClone) {
    nsresult rv = aNode->Clone(nodeInfo, getter_AddRefs(clone), aDeep);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      aError.Throw(rv);
      return nullptr;
    }

    if (CustomElementRegistry::IsCustomElementEnabled(nodeInfo->GetDocument()) &&
        (clone->IsHTMLElement() || clone->IsXULElement())) {
      // The cloned node may be a custom element that may require
      // enqueing upgrade reaction.
      Element* cloneElem = clone->AsElement();
      CustomElementData* data = elem->GetCustomElementData();
      RefPtr<nsAtom> typeAtom = data ? data->GetCustomElementType() : nullptr;

      if (typeAtom) {
        cloneElem->SetCustomElementData(new CustomElementData(typeAtom));

        MOZ_ASSERT(nodeInfo->NameAtom()->Equals(nodeInfo->LocalName()));
        CustomElementDefinition* definition =
          nsContentUtils::LookupCustomElementDefinition(nodeInfo->GetDocument(),
                                                        nodeInfo->NameAtom(),
                                                        nodeInfo->NamespaceID(),
                                                        typeAtom);
        if (definition) {
          nsContentUtils::EnqueueUpgradeReaction(cloneElem, definition);
        }
      }
    }

    if (aParent) {
      // If we're cloning we need to insert the cloned children into the cloned
      // parent.
      rv = aParent->AppendChildTo(static_cast<nsIContent*>(clone.get()),
                                  false);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        aError.Throw(rv);
        return nullptr;
      }
    }
    else if (aDeep && clone->IsDocument()) {
      // After cloning the document itself, we want to clone the children into
      // the cloned document (somewhat like cloning and importing them into the
      // cloned document).
      nodeInfoManager = clone->mNodeInfo->NodeInfoManager();
    }
  }
  else if (nodeInfoManager) {
    nsIDocument* oldDoc = aNode->OwnerDoc();
    bool wasRegistered = false;
    if (elem) {
      oldDoc->ClearBoxObjectFor(elem);
      wasRegistered = oldDoc->UnregisterActivityObserver(elem);
    }

    aNode->mNodeInfo.swap(newNodeInfo);
    if (elem) {
      elem->NodeInfoChanged(oldDoc);
    }

    nsIDocument* newDoc = aNode->OwnerDoc();
    if (newDoc) {
      if (elem && CustomElementRegistry::IsCustomElementEnabled(newDoc)) {
        // Adopted callback must be enqueued whenever a node’s
        // shadow-including inclusive descendants that is custom.
        CustomElementData* data = elem->GetCustomElementData();
        if (data && data->mState == CustomElementData::State::eCustom) {
          LifecycleAdoptedCallbackArgs args = {
            oldDoc,
            newDoc
          };
          nsContentUtils::EnqueueLifecycleCallback(nsIDocument::eAdopted,
                                                   elem, nullptr, &args);
        }
      }

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
          if (elm->MayHaveSelectionChangeEventListener()) {
            window->SetHasSelectionChangeEventListeners();
          }
        }
      }
    }

    if (wasRegistered && oldDoc != newDoc) {
      nsIContent* content = aNode->AsContent();
      if (auto mediaElem = HTMLMediaElement::FromNodeOrNull(content)) {
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
        JSAutoRealm ar(cx, wrapper);
        ReparentWrapper(cx, wrapper, aError);
        if (aError.Failed()) {
          if (wasRegistered) {
            aNode->OwnerDoc()->UnregisterActivityObserver(aNode->AsElement());
          }
          aNode->mNodeInfo.swap(newNodeInfo);
          if (elem) {
            elem->NodeInfoChanged(newDoc);
          }
          if (wasRegistered) {
            aNode->OwnerDoc()->RegisterActivityObserver(aNode->AsElement());
          }
          return nullptr;
        }
      }
    }
  }

  if (aNodesWithProperties && aNode->HasProperties()) {
    bool ok = aNodesWithProperties->AppendObject(aNode);
    MOZ_RELEASE_ASSERT(ok, "Out of memory");
    if (aClone) {
      ok = aNodesWithProperties->AppendObject(clone);
      MOZ_RELEASE_ASSERT(ok, "Out of memory");
    }
  }

  if (aDeep && (!aClone || !aNode->IsAttr())) {
    // aNode's children.
    for (nsIContent* cloneChild = aNode->GetFirstChild();
         cloneChild;
         cloneChild = cloneChild->GetNextSibling()) {
      nsCOMPtr<nsINode> child =
        CloneAndAdopt(cloneChild, aClone, true, nodeInfoManager,
                      aReparentScope, aNodesWithProperties, clone,
                      aError);
      if (NS_WARN_IF(aError.Failed())) {
        return nullptr;
      }
    }
  }

  if (aDeep && aNode->IsElement()) {
    if (aClone) {
      if (clone->OwnerDoc()->IsStaticDocument()) {
        ShadowRoot* originalShadowRoot = aNode->AsElement()->GetShadowRoot();
        if (originalShadowRoot) {
          ShadowRootInit init;
          init.mMode = originalShadowRoot->Mode();
          RefPtr<ShadowRoot> newShadowRoot =
            clone->AsElement()->AttachShadow(init, aError);
          if (NS_WARN_IF(aError.Failed())) {
            return nullptr;
          }

          newShadowRoot->CloneInternalDataFrom(originalShadowRoot);
          for (nsIContent* origChild = originalShadowRoot->GetFirstChild();
               origChild;
               origChild = origChild->GetNextSibling()) {
            nsCOMPtr<nsINode> child =
              CloneAndAdopt(origChild, aClone, aDeep, nodeInfoManager,
                            aReparentScope, aNodesWithProperties, newShadowRoot,
                            aError);
            if (NS_WARN_IF(aError.Failed())) {
              return nullptr;
            }
          }
        }
      }
    } else {
      if (ShadowRoot* shadowRoot = aNode->AsElement()->GetShadowRoot()) {
        nsCOMPtr<nsINode> child =
          CloneAndAdopt(shadowRoot, aClone, aDeep, nodeInfoManager,
                        aReparentScope, aNodesWithProperties, clone,
                        aError);
        if (NS_WARN_IF(aError.Failed())) {
          return nullptr;
        }
      }
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
      nsCOMPtr<nsINode> child =
        CloneAndAdopt(cloneChild, aClone, aDeep, ownerNodeInfoManager,
                      aReparentScope, aNodesWithProperties, cloneContent,
                      aError);
      if (NS_WARN_IF(aError.Failed())) {
        return nullptr;
      }
    }
  }

  return clone.forget();
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
