/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MutationObservers.h"

#include "nsContentUtils.h"
#include "nsCSSPseudoElements.h"
#include "nsINode.h"
#include "nsIContent.h"
#include "nsIContentInlines.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/Element.h"
#include "nsIMutationObserver.h"
#include "mozilla/EventListenerManager.h"
#include "PLDHashTable.h"
#include "nsCOMArray.h"
#include "nsPIDOMWindow.h"
#ifdef MOZ_XUL
#  include "nsXULElement.h"
#endif
#include "nsGenericHTMLElement.h"
#include "mozilla/AnimationTarget.h"
#include "mozilla/Assertions.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Animation.h"
#include "mozilla/dom/KeyframeEffect.h"
#include "mozilla/PresShell.h"
#include "nsWrapperCacheInlines.h"
#include "nsDOMMutationObserver.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/CustomElementRegistry.h"
#include "mozilla/dom/HTMLTemplateElement.h"
#include "mozilla/dom/ShadowRoot.h"

using namespace mozilla;
using namespace mozilla::dom;
using mozilla::AutoJSContext;

enum class IsRemoveNotification {
  Yes,
  No,
};

#ifdef DEBUG
#  define COMPOSED_DOC_DECL \
    const bool wasInComposedDoc = !!node->GetComposedDoc();
#else
#  define COMPOSED_DOC_DECL
#endif

#define CALL_BINDING_MANAGER(func_, params_) \
  do {                                       \
  } while (0)

// This macro expects the ownerDocument of content_ to be in scope as
// |Document* doc|
#define IMPL_MUTATION_NOTIFICATION(func_, content_, params_, remove_)          \
  PR_BEGIN_MACRO                                                               \
  bool needsEnterLeave = doc->MayHaveDOMMutationObservers();                   \
  if (needsEnterLeave) {                                                       \
    nsDOMMutationObserver::EnterMutationHandling();                            \
  }                                                                            \
  nsINode* node = content_;                                                    \
  COMPOSED_DOC_DECL                                                            \
  NS_ASSERTION(node->OwnerDoc() == doc, "Bogus document");                     \
  if (remove_ == IsRemoveNotification::Yes && node->GetComposedDoc()) {        \
    if (PresShell* presShell = doc->GetObservingPresShell()) {                 \
      presShell->func_ params_;                                                \
    }                                                                          \
  }                                                                            \
  CALL_BINDING_MANAGER(func_, params_);                                        \
  nsINode* last;                                                               \
  do {                                                                         \
    nsINode::nsSlots* slots = node->GetExistingSlots();                        \
    if (slots && !slots->mMutationObservers.IsEmpty()) {                       \
      NS_OBSERVER_ARRAY_NOTIFY_OBSERVERS(slots->mMutationObservers,            \
                                         nsIMutationObserver, func_, params_); \
    }                                                                          \
    last = node;                                                               \
    if (ShadowRoot* shadow = ShadowRoot::FromNode(node)) {                     \
      node = shadow->GetHost();                                                \
    } else {                                                                   \
      node = node->GetParentNode();                                            \
    }                                                                          \
  } while (node);                                                              \
  /* Whitelist NativeAnonymousChildListChange removal notifications from       \
   * the assertion since it runs from UnbindFromTree, and thus we don't        \
   * reach the document, but doesn't matter. */                                \
  MOZ_ASSERT((last == doc) == wasInComposedDoc ||                              \
             (remove_ == IsRemoveNotification::Yes &&                          \
              !strcmp(#func_, "NativeAnonymousChildListChange")));             \
  if (remove_ == IsRemoveNotification::No && last == doc) {                    \
    if (PresShell* presShell = doc->GetObservingPresShell()) {                 \
      presShell->func_ params_;                                                \
    }                                                                          \
  }                                                                            \
  if (needsEnterLeave) {                                                       \
    nsDOMMutationObserver::LeaveMutationHandling();                            \
  }                                                                            \
  PR_END_MACRO

#define IMPL_ANIMATION_NOTIFICATION(func_, content_, params_) \
  PR_BEGIN_MACRO                                              \
  bool needsEnterLeave = doc->MayHaveDOMMutationObservers();  \
  if (needsEnterLeave) {                                      \
    nsDOMMutationObserver::EnterMutationHandling();           \
  }                                                           \
  nsINode* node = content_;                                   \
  do {                                                        \
    nsINode::nsSlots* slots = node->GetExistingSlots();       \
    if (slots && !slots->mMutationObservers.IsEmpty()) {      \
      NS_OBSERVER_ARRAY_NOTIFY_OBSERVERS_WITH_QI(             \
          slots->mMutationObservers, nsIMutationObserver,     \
          nsIAnimationObserver, func_, params_);              \
    }                                                         \
    if (ShadowRoot* shadow = ShadowRoot::FromNode(node)) {    \
      node = shadow->GetHost();                               \
    } else {                                                  \
      node = node->GetParentNode();                           \
    }                                                         \
  } while (node);                                             \
  if (needsEnterLeave) {                                      \
    nsDOMMutationObserver::LeaveMutationHandling();           \
  }                                                           \
  PR_END_MACRO

namespace mozilla {
void MutationObservers::NotifyCharacterDataWillChange(
    nsIContent* aContent, const CharacterDataChangeInfo& aInfo) {
  Document* doc = aContent->OwnerDoc();
  IMPL_MUTATION_NOTIFICATION(CharacterDataWillChange, aContent,
                             (aContent, aInfo), IsRemoveNotification::No);
}

void MutationObservers::NotifyCharacterDataChanged(
    nsIContent* aContent, const CharacterDataChangeInfo& aInfo) {
  Document* doc = aContent->OwnerDoc();
  doc->Changed();
  IMPL_MUTATION_NOTIFICATION(CharacterDataChanged, aContent, (aContent, aInfo),
                             IsRemoveNotification::No);
}

void MutationObservers::NotifyAttributeWillChange(Element* aElement,
                                                  int32_t aNameSpaceID,
                                                  nsAtom* aAttribute,
                                                  int32_t aModType) {
  Document* doc = aElement->OwnerDoc();
  IMPL_MUTATION_NOTIFICATION(AttributeWillChange, aElement,
                             (aElement, aNameSpaceID, aAttribute, aModType),
                             IsRemoveNotification::No);
}

void MutationObservers::NotifyAttributeChanged(Element* aElement,
                                               int32_t aNameSpaceID,
                                               nsAtom* aAttribute,
                                               int32_t aModType,
                                               const nsAttrValue* aOldValue) {
  Document* doc = aElement->OwnerDoc();
  doc->Changed();
  IMPL_MUTATION_NOTIFICATION(
      AttributeChanged, aElement,
      (aElement, aNameSpaceID, aAttribute, aModType, aOldValue),
      IsRemoveNotification::No);
}

void MutationObservers::NotifyAttributeSetToCurrentValue(Element* aElement,
                                                         int32_t aNameSpaceID,
                                                         nsAtom* aAttribute) {
  Document* doc = aElement->OwnerDoc();
  IMPL_MUTATION_NOTIFICATION(AttributeSetToCurrentValue, aElement,
                             (aElement, aNameSpaceID, aAttribute),
                             IsRemoveNotification::No);
}

void MutationObservers::NotifyContentAppended(nsIContent* aContainer,
                                              nsIContent* aFirstNewContent) {
  Document* doc = aContainer->OwnerDoc();
  doc->Changed();
  IMPL_MUTATION_NOTIFICATION(ContentAppended, aContainer, (aFirstNewContent),
                             IsRemoveNotification::No);
}

void MutationObservers::NotifyNativeAnonymousChildListChange(
    nsIContent* aContent, bool aIsRemove) {
  Document* doc = aContent->OwnerDoc();
  auto isRemove =
      aIsRemove ? IsRemoveNotification::Yes : IsRemoveNotification::No;
  IMPL_MUTATION_NOTIFICATION(NativeAnonymousChildListChange, aContent,
                             (aContent, aIsRemove), isRemove);
}

void MutationObservers::NotifyContentInserted(nsINode* aContainer,
                                              nsIContent* aChild) {
  MOZ_ASSERT(aContainer->IsContent() || aContainer->IsDocument(),
             "container must be an nsIContent or an Document");
  Document* doc = aContainer->OwnerDoc();
  doc->Changed();
  IMPL_MUTATION_NOTIFICATION(ContentInserted, aContainer, (aChild),
                             IsRemoveNotification::No);
}

void MutationObservers::NotifyContentRemoved(nsINode* aContainer,
                                             nsIContent* aChild,
                                             nsIContent* aPreviousSibling) {
  MOZ_ASSERT(aContainer->IsContent() || aContainer->IsDocument(),
             "container must be an nsIContent or an Document");
  Document* doc = aContainer->OwnerDoc();
  doc->Changed();
  MOZ_ASSERT(aChild->GetParentNode() == aContainer,
             "We expect the parent link to be still around at this point");
  IMPL_MUTATION_NOTIFICATION(ContentRemoved, aContainer,
                             (aChild, aPreviousSibling),
                             IsRemoveNotification::Yes);
}
}  // namespace mozilla

void MutationObservers::NotifyAnimationMutated(
    dom::Animation* aAnimation, AnimationMutationType aMutatedType) {
  MOZ_ASSERT(aAnimation);

  NonOwningAnimationTarget target = aAnimation->GetTargetForAnimation();
  if (!target) {
    return;
  }

  // A pseudo element and its parent element use the same owner doc.
  Document* doc = target.mElement->OwnerDoc();
  if (doc->MayHaveAnimationObservers()) {
    // we use the its parent element as the subject in DOM Mutation Observer.
    Element* elem = target.mElement;
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

void MutationObservers::NotifyAnimationAdded(dom::Animation* aAnimation) {
  NotifyAnimationMutated(aAnimation, AnimationMutationType::Added);
}

void MutationObservers::NotifyAnimationChanged(dom::Animation* aAnimation) {
  NotifyAnimationMutated(aAnimation, AnimationMutationType::Changed);
}

void MutationObservers::NotifyAnimationRemoved(dom::Animation* aAnimation) {
  NotifyAnimationMutated(aAnimation, AnimationMutationType::Removed);
}
