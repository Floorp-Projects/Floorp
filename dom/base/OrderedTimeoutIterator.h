/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_OrderedTimeoutIterator_h__
#define mozilla_dom_OrderedTimeoutIterator_h__

#include "mozilla/RefPtr.h"
#include "mozilla/dom/Timeout.h"
#include "mozilla/dom/TimeoutManager.h"

namespace mozilla {
namespace dom {

// This class implements and iterator which iterates the normal and tracking
// timeouts lists simultaneously in the mWhen order.
class MOZ_STACK_CLASS OrderedTimeoutIterator final {
public:
  typedef TimeoutManager::Timeouts Timeouts;
  typedef Timeouts::TimeoutList    TimeoutList;

  OrderedTimeoutIterator(Timeouts& aNormalTimeouts,
                         Timeouts& aTrackingTimeouts)
    : mNormalTimeouts(aNormalTimeouts.mTimeoutList),
      mTrackingTimeouts(aTrackingTimeouts.mTimeoutList),
      mNormalIter(mNormalTimeouts.getFirst()),
      mTrackingIter(mTrackingTimeouts.getFirst()),
      mKind(Kind::None),
      mUpdateIteratorCalled(true)
  {
  }

  // Return the current timeout and move to the next one.
  // Unless this is the first time calling Next(), you must call
  // UpdateIterator() before calling this method.
  Timeout* Next()
  {
    MOZ_ASSERT(mUpdateIteratorCalled);
    MOZ_ASSERT_IF(mNormalIter, mNormalIter->isInList());
    MOZ_ASSERT_IF(mTrackingIter, mTrackingIter->isInList());

    mUpdateIteratorCalled = false;
    mKind = Kind::None;
    Timeout* timeout = nullptr;
    if (!mNormalIter) {
      if (!mTrackingIter) {
        // We have reached the end of both lists.  Bail out!
        return nullptr;
      } else {
        // We have reached the end of the normal timeout list, select the next
        // tracking timeout.
        timeout = mTrackingIter;
        mKind = Kind::Tracking;
      }
    } else if (!mTrackingIter) {
      // We have reached the end of the tracking timeout list, select the next
      // normal timeout.
      timeout = mNormalIter;
      mKind = Kind::Normal;
    } else {
      // If we have a normal and a tracking timer, return the one with the
      // smaller mWhen (and prefer the timeout with a lower ID in case they are
      // equal.) Otherwise, return whichever iterator has an item left,
      // preferring a non-tracking timeout again.  Note that in practice, even
      // if a web page calls setTimeout() twice in a row, it should get
      // different mWhen values, so in practice we shouldn't fall back to
      // comparing timeout IDs.
      if (mNormalIter && mTrackingIter &&
          (mTrackingIter->When() < mNormalIter->When() ||
           (mTrackingIter->When() == mNormalIter->When() &&
            mTrackingIter->mTimeoutId < mNormalIter->mTimeoutId))) {
        timeout = mTrackingIter;
        mKind = Kind::Tracking;
      } else if (mNormalIter) {
        timeout = mNormalIter;
        mKind = Kind::Normal;
      } else if (mTrackingIter) {
        timeout = mTrackingIter;
        mKind = Kind::Tracking;
      }
    }
    if (!timeout) {
      // We didn't find any suitable iterator.  This can happen for example
      // when getNext() in UpdateIterator() returns nullptr and then Next()
      // gets called.  Bail out!
      return nullptr;
    }

    MOZ_ASSERT(mKind != Kind::None);

    // Record the current timeout we just found.
    mCurrent = timeout;
    MOZ_ASSERT(mCurrent);

    return mCurrent;
  }

  // Prepare the iterator for the next call to Next().
  // This method can be called as many times as needed.  Calling this more than
  // once is helpful in cases where we expect the timeouts list has been
  // modified before we got a chance to call Next().
  void UpdateIterator()
  {
    MOZ_ASSERT(mKind != Kind::None);
    // Update the winning iterator to point to the next element.  Also check to
    // see if the other iterator is still valid, otherwise reset it to the
    // beginning of the list.  This is needed in case a timeout handler removes
    // the timeout pointed to from one of our iterators.
    if (mKind == Kind::Normal) {
      mNormalIter = mCurrent->getNext();
      if (mTrackingIter && !mTrackingIter->isInList()) {
        mTrackingIter = mTrackingTimeouts.getFirst();
      }
    } else {
      mTrackingIter = mCurrent->getNext();
      if (mNormalIter && !mNormalIter->isInList()) {
        mNormalIter = mNormalTimeouts.getFirst();
      }
    }

    mUpdateIteratorCalled = true;
  }

  // This function resets the iterator to a defunct state.  It should only be
  // used when we want to forcefully sever all of the strong references this
  // class holds.
  void Clear()
  {
    // Release all strong references.
    mNormalIter = nullptr;
    mTrackingIter = nullptr;
    mCurrent = nullptr;
    mKind = Kind::None;
    mUpdateIteratorCalled = true;
  }

  // Returns true if the previous call to Next() picked a normal timeout.
  // Cannot be called before Next() has been called.  Note that the result of
  // this method is only affected by Next() and not UpdateIterator(), so calling
  // UpdateIterator() before calling this is allowed.
  bool PickedNormalIter() const
  {
    MOZ_ASSERT(mKind != Kind::None);
    return mKind == Kind::Normal;
  }

  // Returns true if the previous call to Next() picked a tracking timeout.
  // Cannot be called before Next() has been called.  Note that the result of
  // this method is only affected by Next() and not UpdateIterator(), so calling
  // UpdateIterator() before calling this is allowed.
  bool PickedTrackingIter() const
  {
    MOZ_ASSERT(mKind != Kind::None);
    return mKind == Kind::Tracking;
  }

private:
  TimeoutList& mNormalTimeouts;          // The list of normal timeouts.
  TimeoutList& mTrackingTimeouts;        // The list of tracking timeouts.
  RefPtr<Timeout> mNormalIter;           // The iterator over the normal timeout list.
  RefPtr<Timeout> mTrackingIter;         // The iterator over the tracking timeout list.
  RefPtr<Timeout> mCurrent;              // The current timeout that Next() just found.
  enum class Kind { Normal, Tracking, None };
  Kind mKind;                            // The kind of iterator picked the last time.
  DebugOnly<bool> mUpdateIteratorCalled; // Whether we have called UpdateIterator() before calling Next().
};

}
}

#endif
