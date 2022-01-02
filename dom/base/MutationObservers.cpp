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

#define NOTIFY_PRESSHELL(notify_)                            \
  if (PresShell* presShell = doc->GetObservingPresShell()) { \
    notify_(presShell);                                      \
  }

#define DEFINE_NOTIFIERS(func_, params_)                      \
  auto notifyPresShell = [&](PresShell* aPresShell) {         \
    aPresShell->func_ params_;                                \
  };                                                          \
  auto notifyObserver = [&](nsIMutationObserver* aObserver) { \
    aObserver->func_ params_;                                 \
  };

template <typename NotifyObserver>
static inline nsINode* ForEachAncestorObserver(nsINode* aNode,
                                               NotifyObserver& aFunc) {
  nsINode* last;
  nsINode* node = aNode;
  do {
    nsAutoTObserverArray<nsIMutationObserver*, 1>* observers =
        node->GetMutationObservers();
    if (observers && !observers->IsEmpty()) {
      for (nsIMutationObserver* obs : observers->ForwardRange()) {
        aFunc(obs);
      }
    }
    last = node;
    if (ShadowRoot* shadow = ShadowRoot::FromNode(node)) {
      node = shadow->GetHost();
    } else {
      node = node->GetParentNode();
    }
  } while (node);
  return last;
}

enum class IsRemoval { No, Yes };
enum class ShouldAssert { No, Yes };

template <IsRemoval aIsRemoval = IsRemoval::No,
          ShouldAssert aShouldAssert = ShouldAssert::Yes,
          typename NotifyPresShell, typename NotifyObserver>
static inline void Notify(nsINode* aNode, NotifyPresShell& aNotifyPresShell,
                          NotifyObserver& aNotifyObserver) {
  Document* doc = aNode->OwnerDoc();
  nsDOMMutationEnterLeave enterLeave(doc);

  const bool wasConnected = aNode->IsInComposedDoc();
  // For removals, the pres shell gets notified first, since it needs to operate
  // on the "old" DOM shape.
  if (aIsRemoval == IsRemoval::Yes && wasConnected) {
    NOTIFY_PRESSHELL(aNotifyPresShell);
  }
  nsINode* last = ForEachAncestorObserver(aNode, aNotifyObserver);
  // For non-removals, the pres shell gets notified last, since it needs to
  // operate on the "final" DOM shape.
  if (aIsRemoval == IsRemoval::No && last == doc) {
    NOTIFY_PRESSHELL(aNotifyPresShell);
  }
  if (aShouldAssert == ShouldAssert::Yes) {
    MOZ_ASSERT((last == doc) == wasConnected,
               "If we were connected we should notify all ancestors all the "
               "way to the document");
  }
}

#define IMPL_ANIMATION_NOTIFICATION(func_, content_, params_)                \
  PR_BEGIN_MACRO                                                             \
  nsDOMMutationEnterLeave enterLeave(doc);                                   \
  auto forEach = [&](nsIMutationObserver* aObserver) {                       \
    if (nsCOMPtr<nsIAnimationObserver> obs = do_QueryInterface(aObserver)) { \
      obs->func_ params_;                                                    \
    }                                                                        \
  };                                                                         \
  ForEachAncestorObserver(content_, forEach);                                \
  PR_END_MACRO

namespace mozilla {
void MutationObservers::NotifyCharacterDataWillChange(
    nsIContent* aContent, const CharacterDataChangeInfo& aInfo) {
  DEFINE_NOTIFIERS(CharacterDataWillChange, (aContent, aInfo));
  Notify(aContent, notifyPresShell, notifyObserver);
}

void MutationObservers::NotifyCharacterDataChanged(
    nsIContent* aContent, const CharacterDataChangeInfo& aInfo) {
  aContent->OwnerDoc()->Changed();
  DEFINE_NOTIFIERS(CharacterDataChanged, (aContent, aInfo));
  Notify(aContent, notifyPresShell, notifyObserver);
}

void MutationObservers::NotifyAttributeWillChange(Element* aElement,
                                                  int32_t aNameSpaceID,
                                                  nsAtom* aAttribute,
                                                  int32_t aModType) {
  DEFINE_NOTIFIERS(AttributeWillChange,
                   (aElement, aNameSpaceID, aAttribute, aModType));
  Notify(aElement, notifyPresShell, notifyObserver);
}

void MutationObservers::NotifyAttributeChanged(Element* aElement,
                                               int32_t aNameSpaceID,
                                               nsAtom* aAttribute,
                                               int32_t aModType,
                                               const nsAttrValue* aOldValue) {
  aElement->OwnerDoc()->Changed();
  DEFINE_NOTIFIERS(AttributeChanged,
                   (aElement, aNameSpaceID, aAttribute, aModType, aOldValue));
  Notify(aElement, notifyPresShell, notifyObserver);
}

void MutationObservers::NotifyAttributeSetToCurrentValue(Element* aElement,
                                                         int32_t aNameSpaceID,
                                                         nsAtom* aAttribute) {
  DEFINE_NOTIFIERS(AttributeSetToCurrentValue,
                   (aElement, aNameSpaceID, aAttribute));
  Notify(aElement, notifyPresShell, notifyObserver);
}

void MutationObservers::NotifyContentAppended(nsIContent* aContainer,
                                              nsIContent* aFirstNewContent) {
  aContainer->OwnerDoc()->Changed();
  DEFINE_NOTIFIERS(ContentAppended, (aFirstNewContent));
  Notify(aContainer, notifyPresShell, notifyObserver);
}

void MutationObservers::NotifyNativeAnonymousChildListChange(
    nsIContent* aContent, bool aIsRemove) {
  DEFINE_NOTIFIERS(NativeAnonymousChildListChange, (aContent, aIsRemove));
  if (aIsRemove) {
    // We can't actually assert that we reach the document if we're connected,
    // since this notification runs from UnbindFromTree.
    Notify<IsRemoval::Yes, ShouldAssert::No>(aContent, notifyPresShell,
                                             notifyObserver);
  } else {
    Notify(aContent, notifyPresShell, notifyObserver);
  }
}

void MutationObservers::NotifyContentInserted(nsINode* aContainer,
                                              nsIContent* aChild) {
  MOZ_ASSERT(aContainer->IsContent() || aContainer->IsDocument(),
             "container must be an nsIContent or an Document");
  aContainer->OwnerDoc()->Changed();
  DEFINE_NOTIFIERS(ContentInserted, (aChild));
  Notify(aContainer, notifyPresShell, notifyObserver);
}

void MutationObservers::NotifyContentRemoved(nsINode* aContainer,
                                             nsIContent* aChild,
                                             nsIContent* aPreviousSibling) {
  MOZ_ASSERT(aContainer->IsContent() || aContainer->IsDocument(),
             "container must be an nsIContent or an Document");
  aContainer->OwnerDoc()->Changed();
  MOZ_ASSERT(aChild->GetParentNode() == aContainer,
             "We expect the parent link to be still around at this point");
  DEFINE_NOTIFIERS(ContentRemoved, (aChild, aPreviousSibling));
  Notify<IsRemoval::Yes>(aContainer, notifyPresShell, notifyObserver);
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
