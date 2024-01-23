/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AbortFollower_h
#define mozilla_dom_AbortFollower_h

#include "jsapi.h"
#include "nsISupportsImpl.h"
#include "nsTObserverArray.h"
#include "mozilla/WeakPtr.h"

namespace mozilla::dom {

class AbortSignal;
class AbortSignalImpl;

// This class must be implemented by objects who want to follow an
// AbortSignalImpl.
class AbortFollower : public nsISupports {
 public:
  virtual void RunAbortAlgorithm() = 0;

  // This adds strong reference to this follower on the signal, which means
  // you'll need to call Unfollow() to prevent your object from living
  // needlessly longer.
  void Follow(AbortSignalImpl* aSignal);

  // Explicitly call this to let garbage collection happen sooner when the
  // follower finished its work and cannot be aborted anymore.
  void Unfollow();

  bool IsFollowing() const;

  AbortSignalImpl* Signal() const { return mFollowingSignal; }

 protected:
  virtual ~AbortFollower();

  friend class AbortSignalImpl;

  WeakPtr<AbortSignalImpl> mFollowingSignal;
};

/*
 * AbortSignalImpl is a minimal implementation without an associated global
 * and without event dispatching, those are added in AbortSignal.
 * See Bug 1478101
 */
class AbortSignalImpl : public nsISupports, public SupportsWeakPtr {
 public:
  explicit AbortSignalImpl(bool aAborted, JS::Handle<JS::Value> aReason);

  bool Aborted() const;

  // Web IDL Layer
  void GetReason(JSContext* aCx, JS::MutableHandle<JS::Value> aReason);
  // Helper for other DOM code
  JS::Value RawReason() const;

  virtual void SignalAbort(JS::Handle<JS::Value> aReason);

 protected:
  // Subclasses of this class must call these Traverse and Unlink functions
  // during corresponding cycle collection operations.
  static void Traverse(AbortSignalImpl* aSignal,
                       nsCycleCollectionTraversalCallback& cb);

  static void Unlink(AbortSignalImpl* aSignal);

  virtual ~AbortSignalImpl() { UnlinkFollowers(); }

  void SetAborted(JS::Handle<JS::Value> aReason);

  JS::Heap<JS::Value> mReason;

 private:
  friend class AbortFollower;

  void MaybeAssignAbortError(JSContext* aCx);

  void UnlinkFollowers();

  // Raw pointers.  |AbortFollower::Follow| adds to this array, and
  // |AbortFollower::Unfollow| (also called by the destructor) will remove
  // from this array.  Finally, calling |SignalAbort()| will (after running all
  // abort algorithms) empty this and make all contained followers |Unfollow()|.
  nsTObserverArray<RefPtr<AbortFollower>> mFollowers;

  bool mAborted;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_AbortFollower_h
