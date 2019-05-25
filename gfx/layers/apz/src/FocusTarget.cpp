/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/FocusTarget.h"

#include "mozilla/dom/BrowserBridgeChild.h"  // for BrowserBridgeChild
#include "mozilla/dom/EventTarget.h"         // for EventTarget
#include "mozilla/dom/BrowserParent.h"       // for BrowserParent
#include "mozilla/EventDispatcher.h"         // for EventDispatcher
#include "mozilla/layout/RenderFrame.h"      // For RenderFrame
#include "mozilla/PresShell.h"               // For PresShell
#include "nsIContentInlines.h"               // for nsINode::IsEditable()
#include "nsLayoutUtils.h"                   // for nsLayoutUtils

#define ENABLE_FT_LOGGING 0
// #define ENABLE_FT_LOGGING 1

#if ENABLE_FT_LOGGING
#  define FT_LOG(FMT, ...)         \
    printf_stderr("FT (%s): " FMT, \
                  XRE_IsParentProcess() ? "chrome" : "content", __VA_ARGS__)
#else
#  define FT_LOG(...)
#endif

using namespace mozilla::dom;
using namespace mozilla::layout;

namespace mozilla {
namespace layers {

static PresShell* GetRetargetEventPresShell(PresShell* aRootPresShell) {
  MOZ_ASSERT(aRootPresShell);

  // Use the last focused window in this PresShell and its
  // associated PresShell
  nsCOMPtr<nsPIDOMWindowOuter> window =
      aRootPresShell->GetFocusedDOMWindowInOurWindow();
  if (!window) {
    return nullptr;
  }

  RefPtr<Document> retargetEventDoc = window->GetExtantDoc();
  if (!retargetEventDoc) {
    return nullptr;
  }

  return retargetEventDoc->GetPresShell();
}

static bool HasListenersForKeyEvents(nsIContent* aContent) {
  if (!aContent) {
    return false;
  }

  WidgetEvent event(true, eVoidEvent);
  nsTArray<EventTarget*> targets;
  nsresult rv = EventDispatcher::Dispatch(aContent, nullptr, &event, nullptr,
                                          nullptr, nullptr, &targets);
  NS_ENSURE_SUCCESS(rv, false);
  for (size_t i = 0; i < targets.Length(); i++) {
    if (targets[i]->HasNonSystemGroupListenersForUntrustedKeyEvents()) {
      return true;
    }
  }
  return false;
}

static bool HasListenersForNonPassiveKeyEvents(nsIContent* aContent) {
  if (!aContent) {
    return false;
  }

  WidgetEvent event(true, eVoidEvent);
  nsTArray<EventTarget*> targets;
  nsresult rv = EventDispatcher::Dispatch(aContent, nullptr, &event, nullptr,
                                          nullptr, nullptr, &targets);
  NS_ENSURE_SUCCESS(rv, false);
  for (size_t i = 0; i < targets.Length(); i++) {
    if (targets[i]
            ->HasNonPassiveNonSystemGroupListenersForUntrustedKeyEvents()) {
      return true;
    }
  }
  return false;
}

static bool IsEditableNode(nsINode* aNode) {
  return aNode && aNode->IsEditable();
}

FocusTarget::FocusTarget()
    : mSequenceNumber(0),
      mFocusHasKeyEventListeners(false),
      mData(AsVariant(NoFocusTarget())) {}

FocusTarget::FocusTarget(PresShell* aRootPresShell,
                         uint64_t aFocusSequenceNumber)
    : mSequenceNumber(aFocusSequenceNumber),
      mFocusHasKeyEventListeners(false),
      mData(AsVariant(NoFocusTarget())) {
  MOZ_ASSERT(aRootPresShell);
  MOZ_ASSERT(NS_IsMainThread());

  // Key events can be retargeted to a child PresShell when there is an iframe
  RefPtr<PresShell> presShell = GetRetargetEventPresShell(aRootPresShell);

  if (!presShell) {
    FT_LOG("Creating nil target with seq=%" PRIu64
           " (can't find retargeted presshell)\n",
           aFocusSequenceNumber);

    return;
  }

  RefPtr<Document> document = presShell->GetDocument();
  if (!document) {
    FT_LOG("Creating nil target with seq=%" PRIu64 " (no document)\n",
           aFocusSequenceNumber);

    return;
  }

  // Find the focused content and use it to determine whether there are key
  // event listeners or whether key events will be targeted at a different
  // process through a remote browser.
  nsCOMPtr<nsIContent> focusedContent =
      presShell->GetFocusedContentInOurWindow();
  nsCOMPtr<nsIContent> keyEventTarget = focusedContent;

  // If there is no focused element then event dispatch goes to the body of
  // the page if it exists or the root element.
  if (!keyEventTarget) {
    keyEventTarget = document->GetUnfocusedKeyEventTarget();
  }

  // Check if there are key event listeners that could prevent default or change
  // the focus or selection of the page.
  if (gfxPrefs::APZKeyboardPassiveListeners()) {
    mFocusHasKeyEventListeners =
        HasListenersForNonPassiveKeyEvents(keyEventTarget.get());
  } else {
    mFocusHasKeyEventListeners = HasListenersForKeyEvents(keyEventTarget.get());
  }

  // Check if the key event target is content editable or if the document
  // is in design mode.
  if (IsEditableNode(keyEventTarget) || IsEditableNode(document)) {
    FT_LOG("Creating nil target with seq=%" PRIu64
           ", kl=%d (disabling for editable node)\n",
           aFocusSequenceNumber, static_cast<int>(mFocusHasKeyEventListeners));

    return;
  }

  // Check if the key event target is a remote browser
  if (BrowserParent* browserParent = BrowserParent::GetFrom(keyEventTarget)) {
    RenderFrame* rf = browserParent->GetRenderFrame();

    // The globally focused element for scrolling is in a remote layer tree
    if (rf) {
      FT_LOG("Creating reflayer target with seq=%" PRIu64 ", kl=%d, lt=%" PRIu64
             "\n",
             aFocusSequenceNumber, mFocusHasKeyEventListeners,
             rf->GetLayersId());

      mData = AsVariant<LayersId>(rf->GetLayersId());
      return;
    }

    FT_LOG("Creating nil target with seq=%" PRIu64
           ", kl=%d (remote browser missing layers id)\n",
           aFocusSequenceNumber, mFocusHasKeyEventListeners);

    return;
  }

  // Check if the key event target is a remote browser
  if (BrowserBridgeChild* bbc = BrowserBridgeChild::GetFrom(keyEventTarget)) {
    FT_LOG("Creating oopif reflayer target with seq=%" PRIu64
           ", kl=%d, lt=%" PRIu64 "\n",
           aFocusSequenceNumber, mFocusHasKeyEventListeners,
           bbc->GetLayersId());

    mData = AsVariant<LayersId>(bbc->GetLayersId());
    return;
  }

  // The content to scroll is either the focused element or the focus node of
  // the selection. It's difficult to determine if an element is an interactive
  // element requiring async keyboard scrolling to be disabled. So we only
  // allow async key scrolling based on the selection, which doesn't have
  // this problem and is more common.
  if (focusedContent) {
    FT_LOG("Creating nil target with seq=%" PRIu64
           ", kl=%d (disabling for focusing an element)\n",
           aFocusSequenceNumber, mFocusHasKeyEventListeners);

    return;
  }

  nsCOMPtr<nsIContent> selectedContent =
      presShell->GetSelectedContentForScrolling();

  // Gather the scrollable frames that would be scrolled in each direction
  // for this scroll target
  nsIScrollableFrame* horizontal =
      presShell->GetScrollableFrameToScrollForContent(
          selectedContent.get(), ScrollableDirection::Horizontal);
  nsIScrollableFrame* vertical =
      presShell->GetScrollableFrameToScrollForContent(
          selectedContent.get(), ScrollableDirection::Vertical);

  // We might have the globally focused element for scrolling. Gather a ViewID
  // for the horizontal and vertical scroll targets of this element.
  ScrollTargets target;
  target.mHorizontal = nsLayoutUtils::FindIDForScrollableFrame(horizontal);
  target.mVertical = nsLayoutUtils::FindIDForScrollableFrame(vertical);
  if (XRE_IsContentProcess()) {
    target.mHorizontalRenderRoot = gfxUtils::GetContentRenderRoot();
    target.mVerticalRenderRoot = gfxUtils::GetContentRenderRoot();
  } else {
    target.mHorizontalRenderRoot =
        horizontal ? gfxUtils::RecursivelyGetRenderRootForFrame(
                         horizontal->GetScrolledFrame())
                   : wr::RenderRoot::Default;
    target.mVerticalRenderRoot =
        vertical ? gfxUtils::RecursivelyGetRenderRootForFrame(
                       vertical->GetScrolledFrame())
                 : wr::RenderRoot::Default;
  }
  mData = AsVariant(target);

  FT_LOG("Creating scroll target with seq=%" PRIu64 ", kl=%d, h=%" PRIu64
         ", v=%" PRIu64 "\n",
         aFocusSequenceNumber, mFocusHasKeyEventListeners, target.mHorizontal,
         target.mVertical);
}

bool FocusTarget::operator==(const FocusTarget& aRhs) const {
  return mSequenceNumber == aRhs.mSequenceNumber &&
         mFocusHasKeyEventListeners == aRhs.mFocusHasKeyEventListeners &&
         mData == aRhs.mData;
}

const char* FocusTarget::Type() const {
  if (mData.is<LayersId>()) {
    return "LayersId";
  }
  if (mData.is<ScrollTargets>()) {
    return "ScrollTargets";
  }
  if (mData.is<NoFocusTarget>()) {
    return "NoFocusTarget";
  }
  return "<unknown>";
}

}  // namespace layers
}  // namespace mozilla
