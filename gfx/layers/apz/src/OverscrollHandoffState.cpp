/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OverscrollHandoffState.h"

#include <algorithm>              // for std::stable_sort
#include "mozilla/Assertions.h"
#include "AsyncPanZoomController.h"

namespace mozilla {
namespace layers {

OverscrollHandoffChain::~OverscrollHandoffChain() {}

void
OverscrollHandoffChain::Add(AsyncPanZoomController* aApzc)
{
  mChain.push_back(aApzc);
}

struct CompareByScrollPriority
{
  bool operator()(const nsRefPtr<AsyncPanZoomController>& a,
                  const nsRefPtr<AsyncPanZoomController>& b) const
  {
    return a->HasScrollgrab() && !b->HasScrollgrab();
  }
};

void
OverscrollHandoffChain::SortByScrollPriority()
{
  // The sorting being stable ensures that the relative order between
  // non-scrollgrabbing APZCs remains child -> parent.
  // (The relative order between scrollgrabbing APZCs will also remain
  // child -> parent, though that's just an artefact of the implementation
  // and users of 'scrollgrab' should not rely on this.)
  std::stable_sort(mChain.begin(), mChain.end(), CompareByScrollPriority());
}

const nsRefPtr<AsyncPanZoomController>&
OverscrollHandoffChain::GetApzcAtIndex(uint32_t aIndex) const
{
  MOZ_ASSERT(aIndex < Length());
  return mChain[aIndex];
}

uint32_t
OverscrollHandoffChain::IndexOf(const AsyncPanZoomController* aApzc) const
{
  uint32_t i;
  for (i = 0; i < Length(); ++i) {
    if (mChain[i] == aApzc) {
      break;
    }
  }
  return i;
}

void
OverscrollHandoffChain::ForEachApzc(APZCMethod aMethod) const
{
  MOZ_ASSERT(Length() > 0);
  for (uint32_t i = 0; i < Length(); ++i) {
    (mChain[i]->*aMethod)();
  }
}

bool
OverscrollHandoffChain::AnyApzc(APZCPredicate aPredicate) const
{
  MOZ_ASSERT(Length() > 0);
  for (uint32_t i = 0; i < Length(); ++i) {
    if ((mChain[i]->*aPredicate)()) {
      return true;
    }
  }
  return false;
}

void
OverscrollHandoffChain::FlushRepaints() const
{
  ForEachApzc(&AsyncPanZoomController::FlushRepaintForOverscrollHandoff);
}

void
OverscrollHandoffChain::CancelAnimations() const
{
  ForEachApzc(&AsyncPanZoomController::CancelAnimation);
}

void
OverscrollHandoffChain::ClearOverscroll() const
{
  ForEachApzc(&AsyncPanZoomController::ClearOverscroll);
}

void
OverscrollHandoffChain::SnapBackOverscrolledApzc(const AsyncPanZoomController* aStart) const
{
  uint32_t i = IndexOf(aStart);
  for (; i < Length(); ++i) {
    AsyncPanZoomController* apzc = mChain[i];
    if (!apzc->IsDestroyed() && apzc->SnapBackIfOverscrolled()) {
      // At most one APZC from |aStart| onwards can be overscrolled.
      break;
    }
  }

  // In debug builds, verify our assumption that only one APZC from |aStart|
  // onwards is overscrolled.
#ifdef DEBUG
  ++i;
  for (; i < Length(); ++i) {
    AsyncPanZoomController* apzc = mChain[i];
    if (!apzc->IsDestroyed()) {
      MOZ_ASSERT(!apzc->IsOverscrolled());
    }
  }
#endif
}

bool
OverscrollHandoffChain::CanBePanned(const AsyncPanZoomController* aApzc) const
{
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

bool
OverscrollHandoffChain::HasOverscrolledApzc() const
{
  return AnyApzc(&AsyncPanZoomController::IsOverscrolled);
}

bool
OverscrollHandoffChain::HasFastMovingApzc() const
{
  return AnyApzc(&AsyncPanZoomController::IsMovingFast);
}


} // namespace layers
} // namespace mozilla
