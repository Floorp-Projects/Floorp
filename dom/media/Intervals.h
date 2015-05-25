/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef INTERVALS_H
#define INTERVALS_H

#include <algorithm>
#include "mozilla/TypeTraits.h"
#include "nsTArray.h"

// Specialization for nsTArray CopyChooser.
namespace mozilla {
namespace media {
template<class T>
class IntervalSet;
}
}

template<class E>
struct nsTArray_CopyChooser<mozilla::media::IntervalSet<E>>
{
  typedef nsTArray_CopyWithConstructors<mozilla::media::IntervalSet<E>> Type;
};

namespace mozilla {
namespace media {

/* Interval defines an interval between two points. Unlike a traditional
   interval [A,B] where A <= x <= B, the upper boundary B is exclusive: A <= x < B
   (e.g [A,B[ or [A,B) depending on where you're living)
   It provides basic interval arithmetic and fuzzy edges.
   The type T must provides a default constructor and +, -, <, <= and ==
   operators.
 */
template<typename T>
class Interval
{
public:
  typedef Interval<T> SelfType;

  Interval()
    : mStart(T())
    , mEnd(T())
    , mFuzz(T())
  {}

  template<typename StartArg, typename EndArg>
  Interval(StartArg&& aStart, EndArg&& aEnd)
    : mStart(Forward<StartArg>(aStart))
    , mEnd(Forward<EndArg>(aEnd))
    , mFuzz()
  {
    MOZ_ASSERT(aStart <= aEnd);
  }

  template<typename StartArg, typename EndArg, typename FuzzArg>
  Interval(StartArg&& aStart, EndArg&& aEnd, FuzzArg&& aFuzz)
    : mStart(Forward<StartArg>(aStart))
    , mEnd(Forward<EndArg>(aEnd))
    , mFuzz(Forward<FuzzArg>(aFuzz))
  {
    MOZ_ASSERT(aStart <= aEnd);
  }

  Interval(const SelfType& aOther)
    : mStart(aOther.mStart)
    , mEnd(aOther.mEnd)
    , mFuzz(aOther.mFuzz)
  {}

  Interval(SelfType&& aOther)
    : mStart(Move(aOther.mStart))
    , mEnd(Move(aOther.mEnd))
    , mFuzz(Move(aOther.mFuzz))
  { }

  SelfType& operator= (const SelfType& aOther)
  {
    mStart = aOther.mStart;
    mEnd = aOther.mEnd;
    mFuzz = aOther.mFuzz;
    return *this;
  }

  SelfType& operator= (SelfType&& aOther)
  {
    MOZ_ASSERT(&aOther != this, "self-moves are prohibited");
    this->~Interval();
    new(this) Interval(Move(aOther));
    return *this;
  }

  // Basic interval arithmetic operator definition.
  SelfType operator+ (const SelfType& aOther) const
  {
    return SelfType(mStart + aOther.mStart,
                    mEnd + aOther.mEnd,
                    mFuzz + aOther.mFuzz);
  }

  // Basic interval arithmetic operator definition.
  SelfType operator- (const SelfType& aOther) const
  {
    return SelfType(mStart - aOther.mEnd,
                    mEnd - aOther.mStart,
                    mFuzz + aOther.mFuzz);
  }

  bool operator== (const SelfType& aOther) const
  {
    return mStart == aOther.mStart && mEnd == aOther.mEnd;
  }

  bool operator!= (const SelfType& aOther) const
  {
    return !(*this == aOther);
  }

  bool Contains(const T& aX) const
  {
    return mStart - mFuzz <= aX && aX < mEnd + mFuzz;
  }

  bool ContainsStrict(const T& aX) const
  {
    return mStart <= aX && aX < mEnd;
  }

  bool Contains(const SelfType& aOther) const
  {
    return (mStart - mFuzz <= aOther.mStart + aOther.mFuzz) &&
      (aOther.mEnd - aOther.mFuzz <= mEnd + mFuzz);
  }

  bool ContainsStrict(const SelfType& aOther) const
  {
    return mStart <= aOther.mStart && aOther.mEnd <= mEnd;
  }

  bool Intersects(const SelfType& aOther) const
  {
    return (mStart - mFuzz < aOther.mEnd + aOther.mFuzz) &&
      (aOther.mStart - aOther.mFuzz < mEnd + mFuzz);
  }

  // Same as Intersects, but including the boundaries.
  bool Touches(const SelfType& aOther) const
  {
    return (mStart - mFuzz <= aOther.mEnd + aOther.mFuzz) &&
      (aOther.mStart - aOther.mFuzz <= mEnd + mFuzz);
  }

  // Returns true if aOther is strictly to the right of this and contiguous.
  // This operation isn't commutative.
  bool Contiguous(const SelfType& aOther) const
  {
    return mEnd <= aOther.mStart && aOther.mStart - mEnd <= mFuzz + aOther.mFuzz;
  }

  bool RightOf(const SelfType& aOther) const
  {
    return aOther.mEnd - aOther.mFuzz <= mStart + mFuzz;
  }

  bool LeftOf(const SelfType& aOther) const
  {
    return mEnd - mFuzz <= aOther.mStart + aOther.mFuzz;
  }

  SelfType Span(const SelfType& aOther) const
  {
    SelfType result(*this);
    if (aOther.mStart < mStart) {
      result.mStart = aOther.mStart;
    }
    if (mEnd < aOther.mEnd) {
      result.mEnd = aOther.mEnd;
    }
    if (mFuzz < aOther.mFuzz) {
      result.mFuzz = aOther.mFuzz;
    }
    return result;
  }

  SelfType Intersection(const SelfType& aOther) const
  {
    const T& s = std::max(mStart, aOther.mStart);
    const T& e = std::min(mEnd, aOther.mEnd);
    const T& f = std::max(mFuzz, aOther.mFuzz);
    if (s < e) {
      return SelfType(s, e, f);
    }
    // Return an empty interval.
    return SelfType();
  }

  T Length() const
  {
    return mEnd - mStart;
  }

  bool IsEmpty() const
  {
    return mStart == mEnd;
  }

  void SetFuzz(const T& aFuzz)
  {
    mFuzz = aFuzz;
  }

  // Returns true if the two intervals intersect with this being on the right
  // of aOther
  bool TouchesOnRight(const SelfType& aOther) const
  {
    return aOther.mStart <= mStart  &&
      (mStart - mFuzz <= aOther.mEnd + aOther.mFuzz) &&
      (aOther.mStart - aOther.mFuzz <= mEnd + mFuzz);
  }

  T mStart;
  T mEnd;
  T mFuzz;

private:
};

// An IntervalSet in a collection of Intervals. The IntervalSet is always
// normalized.
template<typename T>
class IntervalSet
{
public:
  typedef IntervalSet<T> SelfType;
  typedef Interval<T> ElemType;
  typedef nsAutoTArray<ElemType,4> ContainerType;
  typedef typename ContainerType::index_type IndexType;

  IntervalSet()
  {
  }
  virtual ~IntervalSet()
  {
  }

  IntervalSet(const SelfType& aOther)
    : mIntervals(aOther.mIntervals)
  {
  }

  IntervalSet(SelfType&& aOther)
  {
    mIntervals.MoveElementsFrom(Move(aOther.mIntervals));
  }

  explicit IntervalSet(const ElemType& aOther)
  {
    if (!aOther.IsEmpty()) {
      mIntervals.AppendElement(aOther);
    }
  }

  explicit IntervalSet(ElemType&& aOther)
  {
    if (!aOther.IsEmpty()) {
      mIntervals.AppendElement(Move(aOther));
    }
  }

  SelfType& operator= (const SelfType& aOther)
  {
    mIntervals = aOther.mIntervals;
    return *this;
  }

  SelfType& operator= (SelfType&& aOther)
  {
    MOZ_ASSERT(&aOther != this, "self-moves are prohibited");
    this->~IntervalSet();
    new(this) IntervalSet(Move(aOther));
    return *this;
  }

  SelfType& operator= (const ElemType& aInterval)
  {
    mIntervals.Clear();
    mIntervals.AppendElement(aInterval);
    return *this;
  }

  SelfType& operator= (ElemType&& aInterval)
  {
    mIntervals.Clear();
    mIntervals.AppendElement(Move(aInterval));
    return *this;
  }

  SelfType& Add(const SelfType& aIntervals)
  {
    mIntervals.AppendElements(aIntervals.mIntervals);
    Normalize();
    return *this;
  }

  SelfType& Add(const ElemType& aInterval)
  {
    if (aInterval.IsEmpty()) {
      return *this;
    }
    if (mIntervals.IsEmpty()) {
      mIntervals.AppendElement(aInterval);
      return *this;
    }
    ElemType& last = mIntervals.LastElement();
    if (aInterval.TouchesOnRight(last)) {
      last = last.Span(aInterval);
      return *this;
    }
    // Most of our actual usage is adding an interval that will be outside the
    // range. We can speed up normalization here.
    if (aInterval.RightOf(last)) {
      mIntervals.AppendElement(aInterval);
      return *this;
    }

    ContainerType normalized;
    ElemType current(aInterval);
    bool inserted = false;
    IndexType i = 0;
    for (; i < mIntervals.Length(); i++) {
      ElemType& interval = mIntervals[i];
      if (current.Touches(interval)) {
        current = current.Span(interval);
      } else if (current.LeftOf(interval)) {
        normalized.AppendElement(Move(current));
        inserted = true;
        break;
      } else {
        normalized.AppendElement(Move(interval));
      }
    }
    if (!inserted) {
      normalized.AppendElement(Move(current));
    }
    for (; i < mIntervals.Length(); i++) {
      normalized.AppendElement(Move(mIntervals[i]));
    }
    mIntervals.Clear();
    mIntervals.MoveElementsFrom(Move(normalized));

    return *this;
  }

  SelfType& operator+= (const SelfType& aIntervals)
  {
    Add(aIntervals);
    return *this;
  }

  SelfType& operator+= (const ElemType& aInterval)
  {
    Add(aInterval);
    return *this;
  }

  SelfType operator+ (const SelfType& aIntervals) const
  {
    SelfType intervals(*this);
    intervals.Add(aIntervals);
    return intervals;
  }

  SelfType operator+ (const ElemType& aInterval)
  {
    SelfType intervals(*this);
    intervals.Add(aInterval);
    return intervals;
  }

  friend SelfType operator+ (const ElemType& aInterval,
                             const SelfType& aIntervals)
  {
    SelfType intervals;
    intervals.Add(aInterval);
    intervals.Add(aIntervals);
    return intervals;
  }

  // Mutate this IntervalSet to be the union of this and aOther.
  SelfType& Union(const SelfType& aOther)
  {
    Add(aOther);
    return *this;
  }

  SelfType& Union(const ElemType& aInterval)
  {
    Add(aInterval);
    return *this;
  }

  // Mutate this TimeRange to be the intersection of this and aOther.
  SelfType& Intersection(const SelfType& aOther)
  {
    ContainerType intersection;

    const ContainerType& other = aOther.mIntervals;
    IndexType i = 0, j = 0;
    for (; i < mIntervals.Length() && j < other.Length();) {
      if (mIntervals[i].Intersects(other[j])) {
        intersection.AppendElement(mIntervals[i].Intersection(other[j]));
      }
      if (mIntervals[i].mEnd < other[j].mEnd) {
        i++;
      } else {
        j++;
      }
    }
    mIntervals.Clear();
    mIntervals.MoveElementsFrom(Move(intersection));
    return *this;
  }

  SelfType& Intersection(const ElemType& aInterval)
  {
    SelfType intervals(aInterval);
    return Intersection(intervals);
  }

  const ElemType& operator[] (IndexType aIndex) const
  {
    return mIntervals[aIndex];
  }

  // Returns the start boundary of the first interval. Or a default constructed
  // T if IntervalSet is empty (and aExists if provided will be set to false).
  T GetStart(bool* aExists = nullptr) const
  {
    bool exists = !mIntervals.IsEmpty();

    if (aExists) {
      *aExists = exists;
    }

    if (exists) {
      return mIntervals[0].mStart;
    } else {
      return T();
    }
  }

  // Returns the end boundary of the last interval. Or a default constructed T
  // if IntervalSet is empty (and aExists if provided will be set to false).
  T GetEnd(bool* aExists = nullptr) const
  {
    bool exists = !mIntervals.IsEmpty();
    if (aExists) {
      *aExists = exists;
    }

    if (exists) {
      return mIntervals.LastElement().mEnd;
    } else {
      return T();
    }
  }

  IndexType Length() const
  {
    return mIntervals.Length();
  }

  T Start(IndexType aIndex) const
  {
    return mIntervals[aIndex].mStart;
  }

  T Start(IndexType aIndex, bool& aExists) const
  {
    aExists = aIndex < mIntervals.Length();

    if (aExists) {
      return mIntervals[aIndex].mStart;
    } else {
      return T();
    }
  }

  T End(IndexType aIndex) const
  {
    return mIntervals[aIndex].mEnd;
  }

  T End(IndexType aIndex, bool& aExists) const
  {
    aExists = aIndex < mIntervals.Length();

    if (aExists) {
      return mIntervals[aIndex].mEnd;
    } else {
      return T();
    }
  }

  bool Contains(const ElemType& aInterval) const {
    for (const auto& interval : mIntervals) {
      if (aInterval.LeftOf(interval)) {
        // Will never succeed.
        return false;
      }
      if (interval.Contains(aInterval)) {
        return true;
      }
    }
    return false;
  }

  bool Contains(const T& aX) const {
    for (const auto& interval : mIntervals) {
      if (interval.Contains(aX)) {
        return true;
      }
    }
    return false;
  }

  bool ContainsStrict(const T& aX) const {
    for (const auto& interval : mIntervals) {
      if (interval.ContainsStrict(aX)) {
        return true;
      }
    }
    return false;
  }

  // Shift all values by aOffset.
  void Shift(const T& aOffset)
  {
    for (auto& interval : mIntervals) {
      interval.mStart = interval.mStart + aOffset;
      interval.mEnd = interval.mEnd + aOffset;
    }
  }

  void SetFuzz(const T& aFuzz) {
    for (auto& interval : mIntervals) {
      interval.SetFuzz(aFuzz);
    }
    Normalize();
  }

  static const IndexType NoIndex = IndexType(-1);

  IndexType Find(const T& aValue) const
  {
    for (IndexType i = 0; i < mIntervals.Length(); i++) {
      if (mIntervals[i].Contains(aValue)) {
        return i;
      }
    }
    return NoIndex;
  }

protected:
  ContainerType mIntervals;

private:
  void Normalize()
  {
    if (mIntervals.Length() >= 2) {
      ContainerType normalized;

      mIntervals.Sort(CompareIntervals());

      // This merges the intervals.
      ElemType current(mIntervals[0]);
      for (IndexType i = 1; i < mIntervals.Length(); i++) {
        ElemType& interval = mIntervals[i];
        if (current.Touches(interval)) {
          current = current.Span(interval);
        } else {
          normalized.AppendElement(Move(current));
          current = Move(interval);
        }
      }
      normalized.AppendElement(Move(current));

      mIntervals.Clear();
      mIntervals.MoveElementsFrom(Move(normalized));
    }
  }

  struct CompareIntervals
  {
    bool Equals(const ElemType& aT1, const ElemType& aT2) const
    {
      return aT1.mStart == aT2.mStart && aT1.mEnd == aT2.mEnd;
    }

    bool LessThan(const ElemType& aT1, const ElemType& aT2) const {
      return aT1.mStart - aT1.mFuzz < aT2.mStart + aT2.mFuzz;
    }
  };
};

  // clang doesn't allow for this to be defined inline of IntervalSet.
template<typename T>
IntervalSet<T> Union(const IntervalSet<T>& aIntervals1,
                     const IntervalSet<T>& aIntervals2)
{
  IntervalSet<T> intervals(aIntervals1);
  intervals.Union(aIntervals2);
  return intervals;
}

template<typename T>
IntervalSet<T> Intersection(const IntervalSet<T>& aIntervals1,
                            const IntervalSet<T>& aIntervals2)
{
  IntervalSet<T> intersection(aIntervals1);
  intersection.Intersection(aIntervals2);
  return intersection;
}

} // namespace media
} // namespace mozilla

#endif // INTERVALS_H
