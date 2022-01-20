/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AnimationFrameProvider.h"
#include "nsThreadUtils.h"

namespace mozilla::dom {

FrameRequest::FrameRequest(FrameRequestCallback& aCallback, int32_t aHandle)
    : mCallback(&aCallback), mHandle(aHandle) {
  LogFrameRequestCallback::LogDispatch(mCallback);
}

FrameRequest::~FrameRequest() = default;

nsresult FrameRequestManager::Schedule(FrameRequestCallback& aCallback,
                                       int32_t* aHandle) {
  if (mCallbackCounter == INT32_MAX) {
    // Can't increment without overflowing; bail out
    return NS_ERROR_NOT_AVAILABLE;
  }
  int32_t newHandle = ++mCallbackCounter;

  mCallbacks.AppendElement(FrameRequest(aCallback, newHandle));

  *aHandle = newHandle;
  return NS_OK;
}

bool FrameRequestManager::Cancel(int32_t aHandle) {
  // mCallbacks is stored sorted by handle
  if (mCallbacks.RemoveElementSorted(aHandle)) {
    return true;
  }

  Unused << mCanceledCallbacks.put(aHandle);
  return false;
}

void FrameRequestManager::Unlink() { mCallbacks.Clear(); }

void FrameRequestManager::Traverse(nsCycleCollectionTraversalCallback& aCB) {
  for (auto& i : mCallbacks) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(aCB,
                                       "FrameRequestManager::mCallbacks[i]");
    aCB.NoteXPCOMChild(i.mCallback);
  }
}

}  // namespace mozilla::dom
