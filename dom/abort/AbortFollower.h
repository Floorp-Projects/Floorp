/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AbortFollower_h
#define mozilla_dom_AbortFollower_h

#include "nsISupportsImpl.h"
#include "nsTObserverArray.h"

namespace mozilla {
namespace dom {

class AbortSignal;
class AbortSignalImpl;

// This class must be implemented by objects who want to follow an
// AbortSignalImpl.
class AbortFollower : public nsISupports {
 public:
  virtual void RunAbortAlgorithm() = 0;

  void Follow(AbortSignalImpl* aSignal);

  void Unfollow();

  bool IsFollowing() const;

  AbortSignalImpl* Signal() const { return mFollowingSignal; }

 protected:
  // Subclasses of this class must call these Traverse and Unlink functions
  // during corresponding cycle collection operations.
  static void Traverse(AbortFollower* aFollower,
                       nsCycleCollectionTraversalCallback& cb);

  static void Unlink(AbortFollower* aFollower) { aFollower->Unfollow(); }

  virtual ~AbortFollower();

  friend class AbortSignalImpl;

  RefPtr<AbortSignalImpl> mFollowingSignal;
};

class AbortSignalImpl : public nsISupports {
 public:
  explicit AbortSignalImpl(bool aAborted);

  bool Aborted() const;

  virtual void SignalAbort();

 protected:
  // Subclasses of this class must call these Traverse and Unlink functions
  // during corresponding cycle collection operations.
  static void Traverse(AbortSignalImpl* aSignal,
                       nsCycleCollectionTraversalCallback& cb);

  static void Unlink(AbortSignalImpl* aSignal) {
    // To be filled in shortly.
  }

  virtual ~AbortSignalImpl() = default;

 private:
  friend class AbortFollower;

  // Raw pointers.  |AbortFollower::Follow| adds to this array, and
  // |AbortFollower::Unfollow| (also callbed by the destructor) will remove
  // from this array.  Finally, calling |SignalAbort()| will (after running all
  // abort algorithms) empty this and make all contained followers |Unfollow()|.
  nsTObserverArray<AbortFollower*> mFollowers;

  bool mAborted;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_AbortFollower_h
