/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AnimationFrameProvider_h
#define mozilla_dom_AnimationFrameProvider_h

#include "mozilla/dom/AnimationFrameProviderBinding.h"
#include "mozilla/HashTable.h"
#include "mozilla/RefPtr.h"
#include "nsTArray.h"

namespace mozilla::dom {

struct FrameRequest {
  FrameRequest(FrameRequestCallback& aCallback, int32_t aHandle);
  ~FrameRequest();

  // Comparator operators to allow RemoveElementSorted with an
  // integer argument on arrays of FrameRequest
  bool operator==(int32_t aHandle) const { return mHandle == aHandle; }
  bool operator<(int32_t aHandle) const { return mHandle < aHandle; }

  RefPtr<FrameRequestCallback> mCallback;
  int32_t mHandle;
};

class FrameRequestManager {
 public:
  FrameRequestManager() = default;
  ~FrameRequestManager() = default;

  nsresult Schedule(FrameRequestCallback& aCallback, int32_t* aHandle);
  bool Cancel(int32_t aHandle);

  bool IsEmpty() const { return mCallbacks.IsEmpty(); }

  bool IsCanceled(int32_t aHandle) const {
    return !mCanceledCallbacks.empty() && mCanceledCallbacks.has(aHandle);
  }

  void Take(nsTArray<FrameRequest>& aCallbacks) {
    aCallbacks = std::move(mCallbacks);
    mCanceledCallbacks.clear();
  }

  void Unlink();

  void Traverse(nsCycleCollectionTraversalCallback& aCB);

 private:
  nsTArray<FrameRequest> mCallbacks;

  // The set of frame request callbacks that were canceled but which we failed
  // to find in mFrameRequestCallbacks.
  HashSet<int32_t> mCanceledCallbacks;

  /**
   * The current frame request callback handle
   */
  int32_t mCallbackCounter = 0;
};

inline void ImplCycleCollectionUnlink(FrameRequestManager& aField) {
  aField.Unlink();
}

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback, FrameRequestManager& aField,
    const char* aName, uint32_t aFlags) {
  aField.Traverse(aCallback);
}

}  // namespace mozilla::dom

#endif
