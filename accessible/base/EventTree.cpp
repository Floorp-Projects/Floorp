/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EventTree.h"

#include "EmbeddedObjCollector.h"
#include "NotificationController.h"
#ifdef A11Y_LOG
#  include "Logging.h"
#endif

#include "mozilla/UniquePtr.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// TreeMutation class

TreeMutation::TreeMutation(LocalAccessible* aParent, bool aNoEvents)
    : mParent(aParent),
      mStartIdx(UINT32_MAX),
      mStateFlagsCopy(mParent->mStateFlags),
      mQueueEvents(!aNoEvents) {
#ifdef DEBUG
  mIsDone = false;
#endif

  mParent->mStateFlags |= LocalAccessible::eKidsMutating;
}

TreeMutation::~TreeMutation() {
  MOZ_ASSERT(mIsDone, "Done() must be called explicitly");
}

void TreeMutation::AfterInsertion(LocalAccessible* aChild) {
  MOZ_ASSERT(aChild->LocalParent() == mParent);

  if (static_cast<uint32_t>(aChild->mIndexInParent) < mStartIdx) {
    mStartIdx = aChild->mIndexInParent + 1;
  }

  if (!mQueueEvents) {
    return;
  }

  RefPtr<AccShowEvent> ev = new AccShowEvent(aChild);
  DebugOnly<bool> added = Controller()->QueueMutationEvent(ev);
  MOZ_ASSERT(added);
  aChild->SetShowEventTarget(true);
}

void TreeMutation::BeforeRemoval(LocalAccessible* aChild, bool aNoShutdown) {
  MOZ_ASSERT(aChild->LocalParent() == mParent);

  if (static_cast<uint32_t>(aChild->mIndexInParent) < mStartIdx) {
    mStartIdx = aChild->mIndexInParent;
  }

  if (!mQueueEvents) {
    return;
  }

  RefPtr<AccHideEvent> ev = new AccHideEvent(aChild, !aNoShutdown);
  if (Controller()->QueueMutationEvent(ev)) {
    aChild->SetHideEventTarget(true);
  }
}

void TreeMutation::Done() {
  MOZ_ASSERT(mParent->mStateFlags & LocalAccessible::eKidsMutating);
  mParent->mStateFlags &= ~LocalAccessible::eKidsMutating;

  uint32_t length = mParent->mChildren.Length();
#ifdef DEBUG
  for (uint32_t idx = 0; idx < mStartIdx && idx < length; idx++) {
    MOZ_ASSERT(
        mParent->mChildren[idx]->mIndexInParent == static_cast<int32_t>(idx),
        "Wrong index detected");
  }
#endif

  for (uint32_t idx = mStartIdx; idx < length; idx++) {
    mParent->mChildren[idx]->mIndexOfEmbeddedChild = -1;
  }

  for (uint32_t idx = 0; idx < length; idx++) {
    mParent->mChildren[idx]->mStateFlags |= LocalAccessible::eGroupInfoDirty;
  }

  mParent->mEmbeddedObjCollector = nullptr;
  mParent->mStateFlags |= mStateFlagsCopy & LocalAccessible::eKidsMutating;

#ifdef DEBUG
  mIsDone = true;
#endif
}
