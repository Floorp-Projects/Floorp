/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_OverscrollHandoffChain_h
#define mozilla_layers_OverscrollHandoffChain_h

#include <vector>
#include "nsAutoPtr.h"
#include "nsISupportsImpl.h"  // for NS_INLINE_DECL_REFCOUNTING
#include "Units.h"            // for ScreenPoint

namespace mozilla {
namespace layers {

class AsyncPanZoomController;

/**
 * A variant of NS_INLINE_DECL_THREADSAFE_REFCOUNTING which makes the refcount
 * variable |mutable|, and the AddRef and Release methods |const|, to allow
 * using an |nsRefPtr<const T>|, and thereby enforcing better const-correctness.
 * This is currently here because OverscrollHandoffChain is the only thing
 * currently using it. As a follow-up, we can move this to xpcom/glue, write
 * a corresponding version for non-threadsafe refcounting, and perhaps
 * transition other clients of NS_INLINE_DECL_[THREADSAFE_]REFCOUNTING to the
 * mutable versions.
 */
#define NS_INLINE_DECL_THREADSAFE_MUTABLE_REFCOUNTING(_class)                 \
public:                                                                       \
  NS_METHOD_(MozExternalRefCountType) AddRef(void) const {                    \
    MOZ_ASSERT_TYPE_OK_FOR_REFCOUNTING(_class)                                \
    MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");                      \
    nsrefcnt count = ++mRefCnt;                                               \
    NS_LOG_ADDREF(const_cast<_class*>(this), count, #_class, sizeof(*this));  \
    return (nsrefcnt) count;                                                  \
  }                                                                           \
  NS_METHOD_(MozExternalRefCountType) Release(void) const {                   \
    MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");                          \
    nsrefcnt count = --mRefCnt;                                               \
    NS_LOG_RELEASE(const_cast<_class*>(this), count, #_class);                \
    if (count == 0) {                                                         \
      delete (this);                                                          \
      return 0;                                                               \
    }                                                                         \
    return count;                                                             \
  }                                                                           \
protected:                                                                    \
  mutable ::mozilla::ThreadSafeAutoRefCnt mRefCnt;                            \
public:

/**
 * This class represents the chain of APZCs along which overscroll is handed off.
 * It is created by APZCTreeManager by starting from an initial APZC which is
 * the target for input events, and following the scroll parent ID links (often
 * but not always corresponding to parent pointers in the APZC tree), then
 * adjusting for scrollgrab.
 */
class OverscrollHandoffChain
{
protected:
  // Reference-counted classes cannot have public destructors.
  ~OverscrollHandoffChain();
public:
  // Threadsafe so that the controller and compositor threads can both maintain
  // nsRefPtrs to the same handoff chain.
  // Mutable so that we can pass around the class by
  // nsRefPtr<const OverscrollHandoffChain> and thus enforce that, once built,
  // the chain is not modified.
  NS_INLINE_DECL_THREADSAFE_MUTABLE_REFCOUNTING(OverscrollHandoffChain)

  /*
   * Methods for building the handoff chain.
   * These should be used only by AsyncPanZoomController::BuildOverscrollHandoffChain().
   */
  void Add(AsyncPanZoomController* aApzc);
  void SortByScrollPriority();

  /*
   * Methods for accessing the handoff chain.
   */
  uint32_t Length() const { return mChain.size(); }
  const nsRefPtr<AsyncPanZoomController>& GetApzcAtIndex(uint32_t aIndex) const;
  // Returns Length() if |aApzc| is not on this chain.
  uint32_t IndexOf(const AsyncPanZoomController* aApzc) const;

  /*
   * Convenience methods for performing operations on APZCs in the chain.
   */

  // Flush repaints all the way up the chain.
  void FlushRepaints() const;

  // Cancel animations all the way up the chain.
  void CancelAnimations() const;

  // Clear overscroll all the way up the chain.
  void ClearOverscroll() const;

  // Snap back the APZC that is overscrolled on the subset of the chain from
  // |aStart| onwards, if any.
  void SnapBackOverscrolledApzc(const AsyncPanZoomController* aStart) const;

  // Determine whether the given APZC, or any APZC further in the chain,
  // has room to be panned.
  bool CanBePanned(const AsyncPanZoomController* aApzc) const;

  // Determine whether any APZC along this handoff chain is overscrolled.
  bool HasOverscrolledApzc() const;

  // Determine whether any APZC along this handoff chain is moving fast.
  bool HasFastMovingApzc() const;

private:
  std::vector<nsRefPtr<AsyncPanZoomController>> mChain;

  typedef void (AsyncPanZoomController::*APZCMethod)();
  typedef bool (AsyncPanZoomController::*APZCPredicate)() const;
  void ForEachApzc(APZCMethod aMethod) const;
  bool AnyApzc(APZCPredicate aPredicate) const;
};

/**
 * This class groups the state maintained during overscroll handoff.
 */
struct OverscrollHandoffState {
  OverscrollHandoffState(const OverscrollHandoffChain& aChain,
                         const ScreenPoint& aPanDistance)
      : mChain(aChain), mChainIndex(0), mPanDistance(aPanDistance) {}

  // The chain of APZCs along which we hand off scroll.
  // This is const to indicate that the chain does not change over the
  // course of handoff.
  const OverscrollHandoffChain& mChain;

  // The index of the APZC in the chain that we are currently giving scroll to.
  // This is non-const to indicate that this changes over the course of handoff.
  uint32_t mChainIndex;

  // The total distance since touch-start of the pan that triggered the
  // handoff. This is const to indicate that it does not change over the
  // course of handoff.
  // The x/y components of this are non-negative.
  const ScreenPoint mPanDistance;
};
// Don't pollute other files with this macro for now.
#undef NS_INLINE_DECL_THREADSAFE_MUTABLE_REFCOUNTING

}
}

#endif /* mozilla_layers_OverscrollHandoffChain_h */
