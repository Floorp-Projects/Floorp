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
#include "nsXULElement.h"
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

#define NOTIFY_PRESSHELL(notify_)                            \
  if (PresShell* presShell = doc->GetObservingPresShell()) { \
    notify_(presShell);                                      \
  }

#define NOTIFIER(func_, ...) \
  [&](nsIMutationObserver* aObserver) { aObserver->func_(__VA_ARGS__); }

template <typename NotifyObserver>
static inline nsINode* ForEachAncestorObserver(nsINode* aNode,
                                               NotifyObserver& aFunc,
                                               uint32_t aCallback) {
  nsINode* last;
  nsINode* node = aNode;
  do {
    mozilla::SafeDoublyLinkedList<nsIMutationObserver>* observers =
        node->GetMutationObservers();
    if (observers) {
      for (auto iter = observers->begin(); iter != observers->end(); ++iter) {
        if (iter->IsCallbackEnabled(aCallback)) {
          aFunc(&*iter);
        }
      }
    }
    last = node;
    if (!(node = node->GetParentNode())) {
      if (ShadowRoot* shadow = ShadowRoot::FromNode(last)) {
        node = shadow->GetHost();
      }
    }
  } while (node);
  return last;
}

// Whether to notify to the PresShell about a mutation.
// For removals, the pres shell gets notified first, since it needs to operate
// on the "old" DOM shape.
enum class NotifyPresShell { No, Before, After };

template <NotifyPresShell aNotifyPresShell = NotifyPresShell::After,
          typename NotifyObserver>
static inline void Notify(nsINode* aNode, NotifyObserver&& aNotify,
                          uint32_t aCallback) {
  Document* doc = aNode->OwnerDoc();
  nsDOMMutationEnterLeave enterLeave(doc);

#ifdef DEBUG
  const bool wasConnected = aNode->IsInComposedDoc();
#endif
  if constexpr (aNotifyPresShell == NotifyPresShell::Before) {
    if (aNode->IsInComposedDoc()) {
      NOTIFY_PRESSHELL(aNotify);
    }
  }
  nsINode* last = ForEachAncestorObserver(aNode, aNotify, aCallback);
  // For non-removals, the pres shell gets notified last, since it needs to
  // operate on the "final" DOM shape.
  if constexpr (aNotifyPresShell == NotifyPresShell::After) {
    if (last == doc) {
      NOTIFY_PRESSHELL(aNotify);
    }
  }
  MOZ_ASSERT((last == doc) == wasConnected,
             "If we were connected we should notify all ancestors all the "
             "way to the document");
}

#define IMPL_ANIMATION_NOTIFICATION(func_, content_, params_)                \
  PR_BEGIN_MACRO                                                             \
  nsDOMMutationEnterLeave enterLeave(doc);                                   \
  auto forEach = [&](nsIMutationObserver* aObserver) {                       \
    if (nsCOMPtr<nsIAnimationObserver> obs = do_QueryInterface(aObserver)) { \
      obs->func_ params_;                                                    \
    }                                                                        \
  };                                                                         \
  ForEachAncestorObserver(content_, forEach, nsIMutationObserver::k##func_); \
  PR_END_MACRO

namespace mozilla {
void MutationObservers::NotifyCharacterDataWillChange(
    nsIContent* aContent, const CharacterDataChangeInfo& aInfo) {
  Notify(aContent, NOTIFIER(CharacterDataWillChange, aContent, aInfo),
         nsIMutationObserver::kCharacterDataWillChange);
}

void MutationObservers::NotifyCharacterDataChanged(
    nsIContent* aContent, const CharacterDataChangeInfo& aInfo) {
  aContent->OwnerDoc()->Changed();
  Notify(aContent, NOTIFIER(CharacterDataChanged, aContent, aInfo),
         nsIMutationObserver::kCharacterDataChanged);
}

void MutationObservers::NotifyAttributeWillChange(Element* aElement,
                                                  int32_t aNameSpaceID,
                                                  nsAtom* aAttribute,
                                                  int32_t aModType) {
  Notify(aElement,
         NOTIFIER(AttributeWillChange, aElement, aNameSpaceID, aAttribute,
                  aModType),
         nsIMutationObserver::kAttributeWillChange);
}

void MutationObservers::NotifyAttributeChanged(Element* aElement,
                                               int32_t aNameSpaceID,
                                               nsAtom* aAttribute,
                                               int32_t aModType,
                                               const nsAttrValue* aOldValue) {
  aElement->OwnerDoc()->Changed();
  Notify(aElement,
         NOTIFIER(AttributeChanged, aElement, aNameSpaceID, aAttribute,
                  aModType, aOldValue),
         nsIMutationObserver::kAttributeChanged);
}

void MutationObservers::NotifyAttributeSetToCurrentValue(Element* aElement,
                                                         int32_t aNameSpaceID,
                                                         nsAtom* aAttribute) {
  Notify(
      aElement,
      NOTIFIER(AttributeSetToCurrentValue, aElement, aNameSpaceID, aAttribute),
      nsIMutationObserver::kAttributeSetToCurrentValue);
}

void MutationObservers::NotifyContentAppended(nsIContent* aContainer,
                                              nsIContent* aFirstNewContent) {
  aContainer->OwnerDoc()->Changed();
  Notify(aContainer, NOTIFIER(ContentAppended, aFirstNewContent),
         nsIMutationObserver::kContentAppended);
}

void MutationObservers::NotifyContentInserted(nsINode* aContainer,
                                              nsIContent* aChild) {
  MOZ_ASSERT(aContainer->IsContent() || aContainer->IsDocument(),
             "container must be an nsIContent or an Document");
  aContainer->OwnerDoc()->Changed();
  Notify(aContainer, NOTIFIER(ContentInserted, aChild),
         nsIMutationObserver::kContentInserted);
}

void MutationObservers::NotifyContentRemoved(nsINode* aContainer,
                                             nsIContent* aChild,
                                             nsIContent* aPreviousSibling) {
  MOZ_ASSERT(aContainer->IsContent() || aContainer->IsDocument(),
             "container must be an nsIContent or an Document");
  aContainer->OwnerDoc()->Changed();
  MOZ_ASSERT(aChild->GetParentNode() == aContainer,
             "We expect the parent link to be still around at this point");
  Notify<NotifyPresShell::Before>(
      aContainer, NOTIFIER(ContentRemoved, aChild, aPreviousSibling),
      nsIMutationObserver::kContentRemoved);
}

void MutationObservers::NotifyARIAAttributeDefaultWillChange(
    mozilla::dom::Element* aElement, nsAtom* aAttribute, int32_t aModType) {
  Notify<NotifyPresShell::No>(
      aElement,
      NOTIFIER(ARIAAttributeDefaultWillChange, aElement, aAttribute, aModType),
      nsIMutationObserver::kARIAAttributeDefaultWillChange);
}

void MutationObservers::NotifyARIAAttributeDefaultChanged(
    mozilla::dom::Element* aElement, nsAtom* aAttribute, int32_t aModType) {
  Notify<NotifyPresShell::No>(
      aElement,
      NOTIFIER(ARIAAttributeDefaultChanged, aElement, aAttribute, aModType),
      nsIMutationObserver::kARIAAttributeDefaultChanged);
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
