/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/FocusTarget.h"

#include "mozilla/dom/EventTarget.h" // for EventTarget
#include "mozilla/dom/TabParent.h"   // for TabParent
#include "mozilla/EventDispatcher.h" // for EventDispatcher
#include "mozilla/layout/RenderFrameParent.h" // For RenderFrameParent
#include "nsIPresShell.h"  // for nsIPresShell
#include "nsLayoutUtils.h" // for nsLayoutUtils

using namespace mozilla::dom;
using namespace mozilla::layout;

namespace mozilla {
namespace layers {

static already_AddRefed<nsIPresShell>
GetRetargetEventPresShell(nsIPresShell* aRootPresShell)
{
  MOZ_ASSERT(aRootPresShell);

  // Use the last focused window in this PresShell and its
  // associated PresShell
  nsCOMPtr<nsPIDOMWindowOuter> window =
    aRootPresShell->GetFocusedDOMWindowInOurWindow();
  if (!window) {
    return nullptr;
  }

  nsCOMPtr<nsIDocument> retargetEventDoc = window->GetExtantDoc();
  if (!retargetEventDoc) {
    return nullptr;
  }

  nsCOMPtr<nsIPresShell> presShell = retargetEventDoc->GetShell();
  return presShell.forget();
}

static bool
HasListenersForKeyEvents(nsIContent* aContent)
{
  if (!aContent) {
    return false;
  }

  WidgetEvent event(true, eVoidEvent);
  nsTArray<EventTarget*> targets;
  nsresult rv = EventDispatcher::Dispatch(aContent, nullptr, &event, nullptr,
      nullptr, nullptr, &targets);
  NS_ENSURE_SUCCESS(rv, false);
  for (size_t i = 0; i < targets.Length(); i++) {
    if (targets[i]->HasUntrustedOrNonSystemGroupKeyEventListeners()) {
      return true;
    }
  }
  return false;
}

static bool
IsEditableNode(nsINode* aNode)
{
  return aNode && aNode->IsEditable();
}

FocusTarget::FocusTarget()
  : mSequenceNumber(0)
  , mFocusHasKeyEventListeners(false)
  , mType(FocusTarget::eNone)
{
}

FocusTarget::FocusTarget(nsIPresShell* aRootPresShell,
                         uint64_t aFocusSequenceNumber)
  : mSequenceNumber(aFocusSequenceNumber)
  , mFocusHasKeyEventListeners(false)
{
  MOZ_ASSERT(aRootPresShell);
  MOZ_ASSERT(NS_IsMainThread());

  // Key events can be retargeted to a child PresShell when there is an iframe
  nsCOMPtr<nsIPresShell> presShell = GetRetargetEventPresShell(aRootPresShell);

  // Get the content that should be scrolled for this PresShell, which is
  // the current focused element or the current DOM selection
  nsCOMPtr<nsIContent> scrollTarget = presShell->GetContentForScrolling();

  // Collect event listener information so we can track what is potentially focus
  // changing
  mFocusHasKeyEventListeners = HasListenersForKeyEvents(scrollTarget);

  // Check if the scroll target is a remote browser
  if (TabParent* browserParent = TabParent::GetFrom(scrollTarget)) {
    RenderFrameParent* rfp = browserParent->GetRenderFrame();

    // The globally focused element for scrolling is in a remote layer tree
    if (rfp) {
      mType = FocusTarget::eRefLayer;
      mData.mRefLayerId = rfp->GetLayersId();
      return;
    }

    mType = FocusTarget::eNone;
    return;
  }

  // If the focus isn't on a remote browser then check for scrollable targets
  if (IsEditableNode(scrollTarget) ||
      IsEditableNode(presShell->GetDocument())) {
    mType = FocusTarget::eNone;
    return;
  }

  // Gather the scrollable frames that would be scrolled in each direction
  // for this scroll target
  nsIScrollableFrame* horizontal =
    presShell->GetScrollableFrameToScrollForContent(scrollTarget.get(),
                                                    nsIPresShell::eHorizontal);
  nsIScrollableFrame* vertical =
    presShell->GetScrollableFrameToScrollForContent(scrollTarget.get(),
                                                    nsIPresShell::eVertical);

  // We might have the globally focused element for scrolling. Gather a ViewID for
  // the horizontal and vertical scroll targets of this element.
  mType = FocusTarget::eScrollLayer;
  mData.mScrollTargets.mHorizontal =
    nsLayoutUtils::FindIDForScrollableFrame(horizontal);
  mData.mScrollTargets.mVertical =
    nsLayoutUtils::FindIDForScrollableFrame(vertical);
}

bool
FocusTarget::operator==(const FocusTarget& aRhs) const
{
  if (mSequenceNumber != aRhs.mSequenceNumber ||
      mFocusHasKeyEventListeners != aRhs.mFocusHasKeyEventListeners ||
      mType != aRhs.mType) {
    return false;
  }

  if (mType == FocusTarget::eRefLayer) {
      return mData.mRefLayerId == aRhs.mData.mRefLayerId;
  } else if (mType == FocusTarget::eScrollLayer) {
      return mData.mScrollTargets.mHorizontal == aRhs.mData.mScrollTargets.mHorizontal &&
             mData.mScrollTargets.mVertical == aRhs.mData.mScrollTargets.mVertical;
  }
  return true;
}

} // namespace layers
} // namespace mozilla
