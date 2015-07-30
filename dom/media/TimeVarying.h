/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_TIMEVARYING_H_
#define MOZILLA_TIMEVARYING_H_

#include "nsTArray.h"

namespace mozilla {

/**
 * This is just for CTOR/DTOR tracking. We can't put this in
 * TimeVarying directly because the different template instances have
 * different sizes and that confuses things.
 */
class TimeVaryingBase {
protected:
  TimeVaryingBase()
  {
    MOZ_COUNT_CTOR(TimeVaryingBase);
  }
  ~TimeVaryingBase()
  {
    MOZ_COUNT_DTOR(TimeVaryingBase);
  }
};

/**
 * Objects of this class represent values that can change over time ---
 * a mathematical function of time.
 * Time is the type of time values, T is the value that changes over time.
 * There are a finite set of "change times"; at each change time, the function
 * instantly changes to a new value. ReservedChanges should be set to the
 * expected number of change events that the object is likely to contain.
 * This value should be 0 for all consumers unless you know that a higher value
 * would be a benefit.
 * There is also a "current time" which must always advance (not go backward).
 * The function is constant for all times less than the current time.
 * When the current time is advanced, the value of the function at the new
 * current time replaces the values for all previous times.
 *
 * The implementation records a mCurrent (the value at the current time)
 * and an array of "change times" (greater than the current time) and the
 * new value for each change time. This is a simple but dumb implementation.
 * We maintain the invariant that each change entry in the array must have
 * a different value to the value in the previous change entry (or, for
 * the first change entry, mCurrent).
 */
template <typename Time, typename T, uint32_t ReservedChanges>
class TimeVarying : public TimeVaryingBase {
public:
  explicit TimeVarying(const T& aInitial) : mCurrent(aInitial) {}
  /**
   * This constructor can only be called if mCurrent has a no-argument
   * constructor.
   */
  TimeVarying() : mCurrent() {}

  /**
   * Sets the value for all times >= aTime to aValue.
   */
  void SetAtAndAfter(Time aTime, const T& aValue)
  {
    for (int32_t i = mChanges.Length() - 1; i >= 0; --i) {
      NS_ASSERTION(i == int32_t(mChanges.Length() - 1),
                   "Always considering last element of array");
      if (aTime > mChanges[i].mTime) {
        if (mChanges[i].mValue != aValue) {
          mChanges.AppendElement(Entry(aTime, aValue));
        }
        return;
      }
      if (aTime == mChanges[i].mTime) {
        if ((i > 0 ? mChanges[i - 1].mValue : mCurrent) == aValue) {
          mChanges.RemoveElementAt(i);
          return;
        }
        mChanges[i].mValue = aValue;
        return;
      }
      mChanges.RemoveElementAt(i);
    }
    if (mCurrent == aValue) {
      return;
    }
    mChanges.InsertElementAt(0, Entry(aTime, aValue));
  }
  /**
   * Returns the final value of the function. If aTime is non-null,
   * sets aTime to the time at which the function changes to that final value.
   * If there are no changes after the current time, returns INT64_MIN in aTime.
   */
  const T& GetLast(Time* aTime = nullptr) const
  {
    if (mChanges.IsEmpty()) {
      if (aTime) {
        *aTime = INT64_MIN;
      }
      return mCurrent;
    }
    if (aTime) {
      *aTime = mChanges[mChanges.Length() - 1].mTime;
    }
    return mChanges[mChanges.Length() - 1].mValue;
  }
  /**
   * Returns the value of the function just before time aTime.
   */
  const T& GetBefore(Time aTime) const
  {
    if (mChanges.IsEmpty() || aTime <= mChanges[0].mTime) {
      return mCurrent;
    }
    int32_t changesLength = mChanges.Length();
    if (mChanges[changesLength - 1].mTime < aTime) {
      return mChanges[changesLength - 1].mValue;
    }
    for (uint32_t i = 1; ; ++i) {
      if (aTime <= mChanges[i].mTime) {
        NS_ASSERTION(mChanges[i].mValue != mChanges[i - 1].mValue,
                     "Only changed values appear in array");
        return mChanges[i - 1].mValue;
      }
    }
  }
  /**
   * Returns the value of the function at time aTime.
   * If aEnd is non-null, sets *aEnd to the time at which the function will
   * change from the returned value to a new value, or INT64_MAX if that
   * never happens.
   * If aStart is non-null, sets *aStart to the time at which the function
   * changed to the returned value, or INT64_MIN if that happened at or
   * before the current time.
   *
   * Currently uses a linear search, but could use a binary search.
   */
  const T& GetAt(Time aTime, Time* aEnd = nullptr, Time* aStart = nullptr) const
  {
    if (mChanges.IsEmpty() || aTime < mChanges[0].mTime) {
      if (aStart) {
        *aStart = INT64_MIN;
      }
      if (aEnd) {
        *aEnd = mChanges.IsEmpty() ? INT64_MAX : mChanges[0].mTime;
      }
      return mCurrent;
    }
    int32_t changesLength = mChanges.Length();
    if (mChanges[changesLength - 1].mTime <= aTime) {
      if (aEnd) {
        *aEnd = INT64_MAX;
      }
      if (aStart) {
        *aStart = mChanges[changesLength - 1].mTime;
      }
      return mChanges[changesLength - 1].mValue;
    }

    for (uint32_t i = 1; ; ++i) {
      if (aTime < mChanges[i].mTime) {
        if (aEnd) {
          *aEnd = mChanges[i].mTime;
        }
        if (aStart) {
          *aStart = mChanges[i - 1].mTime;
        }
        NS_ASSERTION(mChanges[i].mValue != mChanges[i - 1].mValue,
                     "Only changed values appear in array");
        return mChanges[i - 1].mValue;
      }
    }
  }
  /**
   * Advance the current time to aTime.
   */
  void AdvanceCurrentTime(Time aTime)
  {
    for (uint32_t i = 0; i < mChanges.Length(); ++i) {
      if (aTime < mChanges[i].mTime) {
        mChanges.RemoveElementsAt(0, i);
        return;
      }
      mCurrent = mChanges[i].mValue;
    }
    mChanges.Clear();
  }
  /**
   * Make all currently pending changes happen aDelta later than their
   * current change times.
   */
  void InsertTimeAtStart(Time aDelta)
  {
    for (uint32_t i = 0; i < mChanges.Length(); ++i) {
      mChanges[i].mTime += aDelta;
    }
  }

  /**
   * Replace the values of this function at aTimeOffset and later with the
   * values of aOther taken from zero, so if aOther is V at time T >= 0
   * then this function will be V at time T + aTimeOffset. aOther's current
   * time must be >= 0.
   */
  void Append(const TimeVarying& aOther, Time aTimeOffset)
  {
    NS_ASSERTION(aOther.mChanges.IsEmpty() || aOther.mChanges[0].mTime >= 0,
                 "Negative time not allowed here");
    NS_ASSERTION(&aOther != this, "Can't self-append");
    SetAtAndAfter(aTimeOffset, aOther.mCurrent);
    for (uint32_t i = 0; i < aOther.mChanges.Length(); ++i) {
      const Entry& e = aOther.mChanges[i];
      SetAtAndAfter(aTimeOffset + e.mTime, e.mValue);
    }
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
  {
    return mChanges.ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

private:
  struct Entry {
    Entry(Time aTime, const T& aValue) : mTime(aTime), mValue(aValue) {}

    // The time at which the value changes to mValue
    Time mTime;
    T mValue;
  };
  nsAutoTArray<Entry, ReservedChanges> mChanges;
  T mCurrent;
};

} // namespace mozilla

#endif /* MOZILLA_TIMEVARYING_H_ */
