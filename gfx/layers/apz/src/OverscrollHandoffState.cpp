/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OverscrollHandoffState.h"

#include <algorithm>  // for std::stable_sort
#include "mozilla/Assertions.h"
#include "mozilla/FloatingPoint.h"
#include "AsyncPanZoomController.h"

namespace mozilla {
namespace layers {

OverscrollHandoffChain::~OverscrollHandoffChain() = default;

void OverscrollHandoffChain::Add(AsyncPanZoomController* aApzc) {
  mChain.push_back(aApzc);
}

struct CompareByScrollPriority {
  bool operator()(const RefPtr<AsyncPanZoomController>& a,
                  const RefPtr<AsyncPanZoomController>& b) const {
    return a->HasScrollgrab() && !b->HasScrollgrab();
  }
};

void OverscrollHandoffChain::SortByScrollPriority() {
  // The sorting being stable ensures that the relative order between
  // non-scrollgrabbing APZCs remains child -> parent.
  // (The relative order between scrollgrabbing APZCs will also remain
  // child -> parent, though that's just an artefact of the implementation
  // and users of 'scrollgrab' should not rely on this.)
  std::stable_sort(mChain.begin(), mChain.end(), CompareByScrollPriority());
}

const RefPtr<AsyncPanZoomController>& OverscrollHandoffChain::GetApzcAtIndex(
    uint32_t aIndex) const {
  MOZ_ASSERT(aIndex < Length());
  return mChain[aIndex];
}

uint32_t OverscrollHandoffChain::IndexOf(
    const AsyncPanZoomController* aApzc) const {
  uint32_t i;
  for (i = 0; i < Length(); ++i) {
    if (mChain[i] == aApzc) {
      break;
    }
  }
  return i;
}

void OverscrollHandoffChain::ForEachApzc(APZCMethod aMethod) const {
  for (uint32_t i = 0; i < Length(); ++i) {
    (mChain[i]->*aMethod)();
  }
}

bool OverscrollHandoffChain::AnyApzc(APZCPredicate aPredicate) const {
  MOZ_ASSERT(Length() > 0);
  for (uint32_t i = 0; i < Length(); ++i) {
    if ((mChain[i]->*aPredicate)()) {
      return true;
    }
  }
  return false;
}

void OverscrollHandoffChain::FlushRepaints() const {
  ForEachApzc(&AsyncPanZoomController::FlushRepaintForOverscrollHandoff);
}

void OverscrollHandoffChain::CancelAnimations(
    CancelAnimationFlags aFlags) const {
  MOZ_ASSERT(Length() > 0);
  for (uint32_t i = 0; i < Length(); ++i) {
    mChain[i]->CancelAnimation(aFlags);
  }
}

void OverscrollHandoffChain::ClearOverscroll() const {
  ForEachApzc(&AsyncPanZoomController::ClearOverscroll);
}

void OverscrollHandoffChain::SnapBackOverscrolledApzc(
    const AsyncPanZoomController* aStart) const {
  uint32_t i = IndexOf(aStart);
  for (; i < Length(); ++i) {
    AsyncPanZoomController* apzc = mChain[i];
    if (!apzc->IsDestroyed()) {
      apzc->SnapBackIfOverscrolled();
    }
  }
}

void OverscrollHandoffChain::SnapBackOverscrolledApzcForMomentum(
    const AsyncPanZoomController* aStart,
    const ParentLayerPoint& aVelocity) const {
  uint32_t i = IndexOf(aStart);
  for (; i < Length(); ++i) {
    AsyncPanZoomController* apzc = mChain[i];
    if (!apzc->IsDestroyed()) {
      apzc->SnapBackIfOverscrolledForMomentum(aVelocity);
    }
  }
}

bool OverscrollHandoffChain::CanBePanned(
    const AsyncPanZoomController* aApzc) const {
  // Find |aApzc| in the handoff chain.
  uint32_t i = IndexOf(aApzc);

  // See whether any APZC in the handoff chain starting from |aApzc|
  // has room to be panned.
  for (uint32_t j = i; j < Length(); ++j) {
    if (mChain[j]->IsPannable()) {
      return true;
    }
  }

  return false;
}

bool OverscrollHandoffChain::CanScrollInDirection(
    const AsyncPanZoomController* aApzc, ScrollDirection aDirection) const {
  // Find |aApzc| in the handoff chain.
  uint32_t i = IndexOf(aApzc);

  // See whether any APZC in the handoff chain starting from |aApzc|
  // has room to scroll in the given direction.
  for (uint32_t j = i; j < Length(); ++j) {
    if (mChain[j]->CanScroll(aDirection)) {
      return true;
    }
  }

  return false;
}

bool OverscrollHandoffChain::HasOverscrolledApzc() const {
  return AnyApzc(&AsyncPanZoomController::IsOverscrolled);
}

bool OverscrollHandoffChain::HasFastFlungApzc() const {
  return AnyApzc(&AsyncPanZoomController::IsFlingingFast);
}

bool OverscrollHandoffChain::HasAutoscrollApzc() const {
  return AnyApzc(&AsyncPanZoomController::IsAutoscroll);
}

RefPtr<AsyncPanZoomController> OverscrollHandoffChain::FindFirstScrollable(
    const InputData& aInput,
    ScrollDirections* aOutAllowedScrollDirections) const {
  // Start by allowing scrolling in both directions. As we do handoff
  // overscroll-behavior may restrict one or both of the directions.
  *aOutAllowedScrollDirections += ScrollDirection::eVertical;
  *aOutAllowedScrollDirections += ScrollDirection::eHorizontal;

  for (size_t i = 0; i < Length(); i++) {
    if (mChain[i]->CanScroll(aInput)) {
      return mChain[i];
    }

    // If there is any directions we allow overscroll effects on the root
    // content APZC (i.e. the overscroll-behavior of the root one is not
    // `none`), we consider the APZC can be scrollable in terms of pan gestures
    // because it causes overscrolling even if it's not able to scroll to the
    // direction.
    if (StaticPrefs::apz_overscroll_enabled() &&
        // FIXME: Bug 1707491: Drop this pan gesture input check.
        aInput.mInputType == PANGESTURE_INPUT && mChain[i]->IsRootContent()) {
      // Check whether the root content APZC is also overscrollable governed by
      // overscroll-behavior in the same directions where we allow scrolling
      // handoff and where we are going to scroll, if it matches we do handoff
      // to the root content APZC.
      // In other words, if the root content is not scrollable, we don't
      // handoff.
      ScrollDirections allowedOverscrollDirections =
          mChain[i]->GetOverscrollableDirections();
      ParentLayerPoint delta = mChain[i]->GetDeltaForEvent(aInput);
      if (mChain[i]->IsZero(delta.x)) {
        allowedOverscrollDirections -= ScrollDirection::eHorizontal;
      }
      if (mChain[i]->IsZero(delta.y)) {
        allowedOverscrollDirections -= ScrollDirection::eVertical;
      }

      allowedOverscrollDirections &= *aOutAllowedScrollDirections;
      if (!allowedOverscrollDirections.isEmpty()) {
        *aOutAllowedScrollDirections = allowedOverscrollDirections;
        return mChain[i];
      }
    }

    *aOutAllowedScrollDirections &= mChain[i]->GetAllowedHandoffDirections();
    if (aOutAllowedScrollDirections->isEmpty()) {
      return nullptr;
    }
  }
  return nullptr;
}

std::tuple<bool, const AsyncPanZoomController*>
OverscrollHandoffChain::ScrollingDownWillMoveDynamicToolbar(
    const AsyncPanZoomController* aApzc) const {
  MOZ_ASSERT(aApzc && !aApzc->IsRootContent(),
             "Should be used for non-root APZC");

  for (uint32_t i = IndexOf(aApzc); i < Length(); i++) {
    if (mChain[i]->IsRootContent()) {
      bool scrollable = mChain[i]->CanVerticalScrollWithDynamicToolbar();
      return {scrollable, scrollable ? mChain[i].get() : nullptr};
    }

    if (mChain[i]->CanScrollDownwards()) {
      return {false, nullptr};
    }
  }

  return {false, nullptr};
}

}  // namespace layers
}  // namespace mozilla
