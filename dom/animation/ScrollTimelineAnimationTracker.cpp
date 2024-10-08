/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollTimelineAnimationTracker.h"

#include "mozilla/dom/Document.h"

namespace mozilla {

NS_IMPL_CYCLE_COLLECTION(ScrollTimelineAnimationTracker, mPendingSet, mDocument)

void ScrollTimelineAnimationTracker::TriggerPendingAnimations() {
  for (RefPtr<dom::Animation>& animation :
       ToTArray<AutoTArray<RefPtr<dom::Animation>, 32>>(mPendingSet)) {
    MOZ_ASSERT(animation->GetTimeline() &&
               !animation->GetTimeline()->IsMonotonicallyIncreasing());
    // FIXME: Trigger now may not be correct because the spec says:
    // If a user agent determines that animation is immediately ready, it may
    // schedule the task (i.e. ResumeAt()) as a microtask such that it runs at
    // the next microtask checkpoint, but it must not perform the task
    // synchronously.
    // Note: So, for now, we put the animation into the tracker, and trigger
    // them immediately until the frames are ready. Using TriggerOnNextTick()
    // for scroll-driven animations may have issues because we don't tick if
    // no one does scroll.
    if (!animation->TryTriggerNow()) {
      // Note: We keep this animation pending even if its timeline is always
      // inactive. It's pretty hard to tell its future status, for example, it's
      // possible that the scroll container is in display:none subtree but the
      // animating element isn't the subtree, then we need to keep tracking the
      // situation until the scroll container gets framed. so in general we make
      // this animation be pending (i.e. not ready) if its scroll-timeline is
      // inactive, and this also matches the current spec definition.
      continue;
    }
    mPendingSet.Remove(animation);
  }
}

}  // namespace mozilla
