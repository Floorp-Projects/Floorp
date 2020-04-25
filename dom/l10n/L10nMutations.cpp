/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "L10nMutations.h"
#include "mozilla/dom/DocumentInlines.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(L10nMutations)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(L10nMutations)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingElements)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPendingElementsHash)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(L10nMutations)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingElements)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPendingElementsHash)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(L10nMutations)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(L10nMutations)
NS_IMPL_CYCLE_COLLECTING_RELEASE(L10nMutations)

L10nMutations::L10nMutations(DOMLocalization* aDOMLocalization)
    : mDOMLocalization(aDOMLocalization) {
  mObserving = true;
}

void L10nMutations::AttributeChanged(Element* aElement, int32_t aNameSpaceID,
                                     nsAtom* aAttribute, int32_t aModType,
                                     const nsAttrValue* aOldValue) {
  if (!mObserving) {
    return;
  }

  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::datal10nid ||
       aAttribute == nsGkAtoms::datal10nargs)) {
    if (IsInRoots(aElement)) {
      L10nElementChanged(aElement);
    }
  }
}

void L10nMutations::ContentAppended(nsIContent* aChild) {
  if (!mObserving) {
    return;
  }

  nsINode* node = aChild;
  if (!IsInRoots(node)) {
    return;
  }

  ErrorResult rv;
  Sequence<OwningNonNull<Element>> elements;
  while (node) {
    if (node->IsElement()) {
      DOMLocalization::GetTranslatables(*node, elements, rv);
    }

    node = node->GetNextSibling();
  }

  for (auto& elem : elements) {
    L10nElementChanged(elem);
  }
}

void L10nMutations::ContentInserted(nsIContent* aChild) {
  if (!mObserving) {
    return;
  }
  ErrorResult rv;
  Sequence<OwningNonNull<Element>> elements;

  if (!aChild->IsElement()) {
    return;
  }
  Element* elem = aChild->AsElement();

  if (!IsInRoots(elem)) {
    return;
  }
  DOMLocalization::GetTranslatables(*aChild, elements, rv);

  for (auto& elem : elements) {
    L10nElementChanged(elem);
  }
}

void L10nMutations::L10nElementChanged(Element* aElement) {
  if (!mPendingElementsHash.Contains(aElement)) {
    mPendingElements.AppendElement(aElement);
    mPendingElementsHash.PutEntry(aElement);
  }

  if (!mRefreshObserver) {
    StartRefreshObserver();
  }
}

void L10nMutations::PauseObserving() { mObserving = false; }

void L10nMutations::ResumeObserving() { mObserving = true; }

void L10nMutations::WillRefresh(mozilla::TimeStamp aTime) {
  StopRefreshObserver();
  FlushPendingTranslations();
}

void L10nMutations::FlushPendingTranslations() {
  if (!mDOMLocalization) {
    return;
  }

  ErrorResult rv;

  Sequence<OwningNonNull<Element>> elements;

  for (auto& elem : mPendingElements) {
    if (!elem->HasAttr(kNameSpaceID_None, nsGkAtoms::datal10nid)) {
      continue;
    }

    if (!elements.AppendElement(*elem, fallible)) {
      mozalloc_handle_oom(0);
    }
  }

  mPendingElementsHash.Clear();
  mPendingElements.Clear();

  RefPtr<Promise> promise = mDOMLocalization->TranslateElements(elements, rv);
}

void L10nMutations::Disconnect() {
  StopRefreshObserver();
  mDOMLocalization = nullptr;
}

void L10nMutations::StartRefreshObserver() {
  if (!mDOMLocalization || mRefreshObserver) {
    return;
  }

  if (!mRefreshDriver) {
    nsPIDOMWindowInner* innerWindow =
        mDOMLocalization->GetParentObject()->AsInnerWindow();
    Document* doc = innerWindow ? innerWindow->GetExtantDoc() : nullptr;
    if (doc) {
      nsPresContext* ctx = doc->GetPresContext();
      if (ctx) {
        mRefreshDriver = ctx->RefreshDriver();
      }
    }
  }

  // If we can't start the refresh driver, it means
  // that the presContext is not available yet.
  // In that case, we'll trigger the flush of pending
  // elements in Document::CreatePresShell.
  if (mRefreshDriver) {
    mRefreshDriver->AddRefreshObserver(this, FlushType::Style);
    mRefreshObserver = true;
  } else {
    NS_WARNING("[l10n][mutations] Failed to start a refresh observer.");
  }
}

void L10nMutations::StopRefreshObserver() {
  if (!mDOMLocalization) {
    return;
  }

  if (mRefreshDriver) {
    mRefreshDriver->RemoveRefreshObserver(this, FlushType::Style);
    mRefreshObserver = false;
  }
}

void L10nMutations::OnCreatePresShell() {
  if (!mPendingElements.IsEmpty()) {
    StartRefreshObserver();
  }
}

bool L10nMutations::IsInRoots(nsINode* aNode) {
  // If the root of the mutated element is in the light DOM,
  // we know it must be covered by our observer directly.
  //
  // Otherwise, we need to check if its subtree root is the same
  // as any of the `DOMLocalization::mRoots` subtree roots.
  nsINode* root = aNode->SubtreeRoot();

  // If element is in light DOM, it must be covered by one of
  // the DOMLocalization roots to end up here.
  MOZ_ASSERT_IF(!root->IsShadowRoot(),
                mDOMLocalization->SubtreeRootInRoots(root));

  return !root->IsShadowRoot() || mDOMLocalization->SubtreeRootInRoots(root);
}
