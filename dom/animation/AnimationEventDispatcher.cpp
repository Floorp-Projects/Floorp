/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AnimationEventDispatcher.h"

#include "mozilla/EventDispatcher.h"
#include "nsRefreshDriver.h"

namespace mozilla {

NS_IMPL_CYCLE_COLLECTION_CLASS(AnimationEventDispatcher)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AnimationEventDispatcher)
  tmp->ClearEventQueue();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AnimationEventDispatcher)
  for (auto& info : tmp->mPendingEvents) {
    ImplCycleCollectionTraverse(cb, info.mElement,
      "mozilla::AnimationEventDispatcher.mPendingEvents.mElement");
    ImplCycleCollectionTraverse(cb, info.mAnimation,
      "mozilla::AnimationEventDispatcher.mPendingEvents.mAnimation");
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(AnimationEventDispatcher, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(AnimationEventDispatcher, Release)

void
AnimationEventDispatcher::Disconnect()
{
  if (mIsObserving) {
    MOZ_ASSERT(mPresContext && mPresContext->RefreshDriver(),
               "The pres context and the refresh driver should be still "
               "alive if we haven't disassociated from the refresh driver");
    mPresContext->RefreshDriver()->CancelPendingAnimationEvents(this);
    mIsObserving = false;
  }
  mPresContext = nullptr;
}

void
AnimationEventDispatcher::QueueEvents(nsTArray<AnimationEventInfo>&& aEvents)
{
  MOZ_ASSERT(mPresContext,
             "The pres context should be valid");

  mPendingEvents.AppendElements(std::move(aEvents));
  mIsSorted = false;
  if (!mIsObserving) {
    mPresContext->RefreshDriver()->ScheduleAnimationEventDispatch(this);
    mIsObserving = true;
  }
}

} // namespace mozilla

