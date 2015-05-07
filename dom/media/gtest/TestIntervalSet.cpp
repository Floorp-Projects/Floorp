/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/dom/TimeRanges.h"
#include "TimeUnits.h"
#include "Intervals.h"
#include <algorithm>
#include <vector>

using namespace mozilla;

typedef media::Interval<uint8_t> ByteInterval;
typedef media::Interval<int> IntInterval;

ByteInterval CreateByteInterval(int32_t aStart, int32_t aEnd)
{
  ByteInterval test(aStart, aEnd);
  return test;
}

media::IntervalSet<uint8_t> CreateByteIntervalSet(int32_t aStart, int32_t aEnd)
{
  media::IntervalSet<uint8_t> test;
  test += ByteInterval(aStart, aEnd);
  return test;
}

TEST(IntervalSet, Constructors)
{
  const int32_t start = 1;
  const int32_t end = 2;
  const int32_t fuzz = 0;

  // Compiler exercise.
  ByteInterval test1(start, end);
  ByteInterval test2(test1);
  ByteInterval test3(start, end, fuzz);
  ByteInterval test4(test3);
  ByteInterval test5 = CreateByteInterval(start, end);

  media::IntervalSet<uint8_t> blah1(test1);
  media::IntervalSet<uint8_t> blah2 = blah1;
  media::IntervalSet<uint8_t> blah3 = blah1 + test1;
  media::IntervalSet<uint8_t> blah4 = test1 + blah1;
  media::IntervalSet<uint8_t> blah5 = CreateByteIntervalSet(start, end);
  (void)test1; (void)test2; (void)test3; (void)test4; (void)test5;
  (void)blah1; (void)blah2; (void)blah3; (void)blah4; (void)blah5;
}

media::TimeInterval CreateTimeInterval(int32_t aStart, int32_t aEnd)
{
  // Copy constructor test
  media::Microseconds startus(aStart);
  media::TimeUnit start(startus);
  media::TimeUnit end;
  // operator=  test
  end = media::Microseconds(aEnd);
  media::TimeInterval ti(start, end);
  return ti;
}

media::TimeIntervals CreateTimeIntervals(int32_t aStart, int32_t aEnd)
{
  media::TimeIntervals test;
  test += CreateTimeInterval(aStart, aEnd);
  return test;
}

TEST(IntervalSet, TimeIntervalsConstructors)
{
  const media::Microseconds start(1);
  const media::Microseconds end(2);
  const media::Microseconds fuzz;

  // Compiler exercise.
  media::TimeInterval test1(start, end);
  media::TimeInterval test2(test1);
  media::TimeInterval test3(start, end, fuzz);
  media::TimeInterval test4(test3);
  media::TimeInterval test5 = CreateTimeInterval(start.mValue, end.mValue);

  media::TimeIntervals blah1(test1);
  media::TimeIntervals blah2(blah1);
  media::TimeIntervals blah3 = blah1 + test1;
  media::TimeIntervals blah4 = test1 + blah1;
  media::TimeIntervals blah5 = CreateTimeIntervals(start.mValue, end.mValue);
  (void)test1; (void)test2; (void)test3; (void)test4; (void)test5;
  (void)blah1; (void)blah2; (void)blah3; (void)blah4; (void)blah5;
}

TEST(IntervalSet, Length)
{
  IntInterval i(15, 25);
  EXPECT_EQ(10, i.Length());
}

TEST(IntervalSet, Intersection)
{
  IntInterval i0(10, 20);
  IntInterval i1(15, 25);
  IntInterval i = i0.Intersection(i1);
  EXPECT_EQ(15, i.mStart);
  EXPECT_EQ(20, i.mEnd);
}

TEST(IntervalSet, Equals)
{
  IntInterval i0(10, 20);
  IntInterval i1(10, 20);
  EXPECT_EQ(i0, i1);

  IntInterval i2(5, 20);
  EXPECT_NE(i0, i2);

  IntInterval i3(10, 15);
  EXPECT_NE(i0, i2);
}

TEST(IntervalSet, IntersectionIntervalSet)
{
  media::IntervalSet<int> i0;
  i0 += IntInterval(5, 10);
  i0 += IntInterval(20, 25);
  i0 += IntInterval(40, 60);

  media::IntervalSet<int> i1;
  i1.Add(IntInterval(7, 15));
  i1.Add(IntInterval(16, 27));
  i1.Add(IntInterval(45, 50));
  i1.Add(IntInterval(53, 57));

  media::IntervalSet<int> i = media::Intersection(i0, i1);

  EXPECT_EQ(4u, i.Length());

  EXPECT_EQ(7, i[0].mStart);
  EXPECT_EQ(10, i[0].mEnd);

  EXPECT_EQ(20, i[1].mStart);
  EXPECT_EQ(25, i[1].mEnd);

  EXPECT_EQ(45, i[2].mStart);
  EXPECT_EQ(50, i[2].mEnd);

  EXPECT_EQ(53, i[3].mStart);
  EXPECT_EQ(57, i[3].mEnd);
}

template<typename T>
static void Compare(media::IntervalSet<T> aI1, media::IntervalSet<T> aI2)
{
  media::IntervalSet<T> i1(aI1);
  media::IntervalSet<T> i2(aI1);
  EXPECT_EQ(i1.Length(), i2.Length());
  if (i1.Length() != i2.Length()) {
    return;
  }
  for (uint32_t i = 0; i < i1.Length(); i++) {
    EXPECT_EQ(i1[i].mStart, i2[i].mStart);
    EXPECT_EQ(i1[i].mEnd, i2[i].mEnd);
  }
}

TEST(IntervalSet, IntersectionNormalizedIntervalSet)
{
  media::IntervalSet<int> i0;
  i0 += IntInterval(5, 10);
  i0 += IntInterval(8, 25);
  i0 += IntInterval(24, 60);

  media::IntervalSet<int> i1;
  i1.Add(IntInterval(7, 15));
  i1.Add(IntInterval(10, 27));
  i1.Add(IntInterval(45, 50));
  i1.Add(IntInterval(53, 57));

  // Compare intersections to ensure an intersection of normalized intervalsets
  // is equal to the intersection of non-normalized intervalsets.
  media::IntervalSet<int> intersection = media::Intersection(i0, i1);

  media::IntervalSet<int> i0_normalize(i0);
  i0_normalize.Normalize();
  media::IntervalSet<int> i1_normalize(i1);
  i1_normalize.Normalize();
  media::IntervalSet<int> intersection_normalize =
  media::Intersection(i0_normalize, i1_normalize);
  Compare(intersection, intersection_normalize);
}

static void GeneratePermutations(media::IntervalSet<int> aI1,
                                 media::IntervalSet<int> aI2)
{
  media::IntervalSet<int> i_ref = media::Intersection(aI1, aI2);
  // Test all permutations possible
  std::vector<uint32_t> comb1;
  for (uint32_t i = 0; i < aI1.Length(); i++) {
    comb1.push_back(i);
  }
  std::vector<uint32_t> comb2;
  for (uint32_t i = 0; i < aI2.Length(); i++) {
    comb2.push_back(i);
  }

  do {
    do {
      // Create intervals according to new indexes.
      media::IntervalSet<int> i_0;
      for (uint32_t i = 0; i < comb1.size(); i++) {
        i_0 += aI1[comb1[i]];
      }
      media::IntervalSet<int> i_1;
      for (uint32_t i = 0; i < comb2.size(); i++) {
        i_1 += aI2[comb2[i]];
      }
      // Check intersections yield the same result.
      Compare(i_0.Intersection(i_1), i_ref);
    } while (std::next_permutation(comb2.begin(), comb2.end()));
  } while (std::next_permutation(comb1.begin(), comb1.end()));
}

TEST(IntervalSet, IntersectionUnorderedIntervalSet)
{
  media::IntervalSet<int> i0;
  i0 += IntInterval(5, 10);
  i0 += IntInterval(20, 25);
  i0 += IntInterval(40, 60);

  media::IntervalSet<int> i1;
  i1.Add(IntInterval(7, 15));
  i1.Add(IntInterval(16, 27));
  i1.Add(IntInterval(45, 50));
  i1.Add(IntInterval(53, 57));

  GeneratePermutations(i0, i1);
}

TEST(IntervalSet, IntersectionUnorderedNonNormalizedIntervalSet)
{
  media::IntervalSet<int> i0;
  i0 += IntInterval(5, 10);
  i0 += IntInterval(8, 25);
  i0 += IntInterval(24, 60);

  media::IntervalSet<int> i1;
  i1.Add(IntInterval(7, 15));
  i1.Add(IntInterval(10, 27));
  i1.Add(IntInterval(45, 50));
  i1.Add(IntInterval(53, 57));

  GeneratePermutations(i0, i1);
}

static media::IntervalSet<int> Duplicate(const media::IntervalSet<int>& aValue)
{
  media::IntervalSet<int> value(aValue);
  return value;
}

TEST(IntervalSet, Normalize)
{
  media::IntervalSet<int> i;
  // Test IntervalSet<T> + Interval<T> operator.
  i = i + IntInterval(20, 30);
  // Test Internal<T> + IntervalSet<T> operator.
  i = IntInterval(2, 7) + i;
  // Test Interval<T> + IntervalSet<T> operator
  i = IntInterval(1, 8) + i;
  media::IntervalSet<int> interval;
  interval += IntInterval(5, 10);
  // Test += with move.
  i += Duplicate(interval);
  // Test = with move and add with move.
  i = Duplicate(interval) + i;

  media::IntervalSet<int> o(i);
  o.Normalize();

  EXPECT_EQ(2u, o.Length());

  EXPECT_EQ(1, o[0].mStart);
  EXPECT_EQ(10, o[0].mEnd);

  EXPECT_EQ(20, o[1].mStart);
  EXPECT_EQ(30, o[1].mEnd);
}

TEST(IntervalSet, Union)
{
  media::IntervalSet<int> i0;
  i0 += IntInterval(5, 10);
  i0 += IntInterval(20, 25);
  i0 += IntInterval(40, 60);

  media::IntervalSet<int> i1;
  i1.Add(IntInterval(7, 15));
  i1.Add(IntInterval(16, 27));
  i1.Add(IntInterval(45, 50));
  i1.Add(IntInterval(53, 57));

  media::IntervalSet<int> i = media::Union(i0, i1);

  EXPECT_EQ(3u, i.Length());

  EXPECT_EQ(5, i[0].mStart);
  EXPECT_EQ(15, i[0].mEnd);

  EXPECT_EQ(16, i[1].mStart);
  EXPECT_EQ(27, i[1].mEnd);

  EXPECT_EQ(40, i[2].mStart);
  EXPECT_EQ(60, i[2].mEnd);
}

TEST(IntervalSet, UnionNotOrdered)
{
  media::IntervalSet<int> i0;
  i0 += IntInterval(20, 25);
  i0 += IntInterval(40, 60);
  i0 += IntInterval(5, 10);

  media::IntervalSet<int> i1;
  i1.Add(IntInterval(16, 27));
  i1.Add(IntInterval(7, 15));
  i1.Add(IntInterval(53, 57));
  i1.Add(IntInterval(45, 50));

  media::IntervalSet<int> i = media::Union(i0, i1);

  EXPECT_EQ(3u, i.Length());

  EXPECT_EQ(5, i[0].mStart);
  EXPECT_EQ(15, i[0].mEnd);

  EXPECT_EQ(16, i[1].mStart);
  EXPECT_EQ(27, i[1].mEnd);

  EXPECT_EQ(40, i[2].mStart);
  EXPECT_EQ(60, i[2].mEnd);
}

TEST(IntervalSet, NormalizeFuzz)
{
  media::IntervalSet<int> i0;
  i0 += IntInterval(11, 25, 0);
  i0 += IntInterval(5, 10, 1);
  i0 += IntInterval(40, 60, 1);
  i0.Normalize();

  EXPECT_EQ(2u, i0.Length());

  EXPECT_EQ(5, i0[0].mStart);
  EXPECT_EQ(25, i0[0].mEnd);

  EXPECT_EQ(40, i0[1].mStart);
  EXPECT_EQ(60, i0[1].mEnd);
}

TEST(IntervalSet, UnionFuzz)
{
  media::IntervalSet<int> i0;
  i0 += IntInterval(5, 10, 1);
  i0 += IntInterval(11, 25, 0);
  i0 += IntInterval(40, 60, 1);

  media::IntervalSet<int> i1;
  i1.Add(IntInterval(7, 15, 1));
  i1.Add(IntInterval(16, 27, 1));
  i1.Add(IntInterval(45, 50, 1));
  i1.Add(IntInterval(53, 57, 1));

  media::IntervalSet<int> i = media::Union(i0, i1);

  EXPECT_EQ(2u, i.Length());

  EXPECT_EQ(5, i[0].mStart);
  EXPECT_EQ(27, i[0].mEnd);

  EXPECT_EQ(40, i[1].mStart);
  EXPECT_EQ(60, i[1].mEnd);

  i0.Normalize();
  EXPECT_EQ(2u, i0.Length());
  EXPECT_EQ(5, i0[0].mStart);
  EXPECT_EQ(25, i0[0].mEnd);
  EXPECT_EQ(40, i0[1].mStart);
  EXPECT_EQ(60, i0[1].mEnd);
}

TEST(IntervalSet, Contiguous)
{
  EXPECT_FALSE(IntInterval(5, 10).Contiguous(IntInterval(11, 25)));
  EXPECT_TRUE(IntInterval(5, 10).Contiguous(IntInterval(10, 25)));
  EXPECT_TRUE(IntInterval(5, 10, 1).Contiguous(IntInterval(11, 25)));
  EXPECT_TRUE(IntInterval(5, 10).Contiguous(IntInterval(11, 25, 1)));
}

TEST(IntervalSet, TimeRangesSeconds)
{
  media::TimeIntervals i0;
  i0 += media::TimeInterval(media::TimeUnit::FromSeconds(20), media::TimeUnit::FromSeconds(25));
  i0 += media::TimeInterval(media::TimeUnit::FromSeconds(40), media::TimeUnit::FromSeconds(60));
  i0 += media::TimeInterval(media::TimeUnit::FromSeconds(5), media::TimeUnit::FromSeconds(10));

  media::TimeIntervals i1;
  i1.Add(media::TimeInterval(media::TimeUnit::FromSeconds(16), media::TimeUnit::FromSeconds(27)));
  i1.Add(media::TimeInterval(media::TimeUnit::FromSeconds(7), media::TimeUnit::FromSeconds(15)));
  i1.Add(media::TimeInterval(media::TimeUnit::FromSeconds(53), media::TimeUnit::FromSeconds(57)));
  i1.Add(media::TimeInterval(media::TimeUnit::FromSeconds(45), media::TimeUnit::FromSeconds(50)));

  media::TimeIntervals i(i0 + i1);
  nsRefPtr<dom::TimeRanges> tr = new dom::TimeRanges();
  i.ToTimeRanges(tr);
  EXPECT_EQ(tr->Length(), i.Length());
  for (dom::TimeRanges::index_type index = 0; index < tr->Length(); index++) {
    ErrorResult rv;
    EXPECT_EQ(tr->Start(index, rv), i[index].mStart.ToSeconds());
    EXPECT_EQ(tr->Start(index, rv), i.Start(index).ToSeconds());
    EXPECT_EQ(tr->End(index, rv), i[index].mEnd.ToSeconds());
    EXPECT_EQ(tr->End(index, rv), i.End(index).ToSeconds());
  }

  i.Normalize();
  tr->Normalize();
  EXPECT_EQ(tr->Length(), i.Length());
  for (dom::TimeRanges::index_type index = 0; index < tr->Length(); index++) {
    ErrorResult rv;
    EXPECT_EQ(tr->Start(index, rv), i[index].mStart.ToSeconds());
    EXPECT_EQ(tr->Start(index, rv), i.Start(index).ToSeconds());
    EXPECT_EQ(tr->End(index, rv), i[index].mEnd.ToSeconds());
    EXPECT_EQ(tr->End(index, rv), i.End(index).ToSeconds());
  }
}

static void CheckTimeRanges(dom::TimeRanges* aTr, const media::TimeIntervals& aTi)
{
  EXPECT_EQ(aTr->Length(), aTi.Length());
  for (dom::TimeRanges::index_type i = 0; i < aTr->Length(); i++) {
    ErrorResult rv;
    EXPECT_EQ(aTr->Start(i, rv), aTi[i].mStart.ToSeconds());
    EXPECT_EQ(aTr->Start(i, rv), aTi.Start(i).ToSeconds());
    EXPECT_EQ(aTr->End(i, rv), aTi[i].mEnd.ToSeconds());
    EXPECT_EQ(aTr->End(i, rv), aTi.End(i).ToSeconds());
  }
}

TEST(IntervalSet, TimeRangesConversion)
{
  nsRefPtr<dom::TimeRanges> tr = new dom::TimeRanges();
  tr->Add(20, 25);
  tr->Add(40, 60);
  tr->Add(5, 10);
  tr->Add(16, 27);
  tr->Add(53, 57);
  tr->Add(45, 50);

  // explicit copy constructor
  media::TimeIntervals i1(tr);
  CheckTimeRanges(tr, i1);

  // static FromTimeRanges
  media::TimeIntervals i2 = media::TimeIntervals::FromTimeRanges(tr);
  CheckTimeRanges(tr, i2);

  media::TimeIntervals i3;
  // operator=(TimeRanges*)
  i3 = tr;
  CheckTimeRanges(tr, i3);

  i1.Normalize();
  tr->Normalize();

  CheckTimeRanges(tr, i1);

  // operator= test
  i1 = tr.get();
  CheckTimeRanges(tr, i1);
}

TEST(IntervalSet, TimeRangesMicroseconds)
{
  media::TimeIntervals i0;

  // Test media::Microseconds and TimeUnit interchangeability (compilation only)
  media::TimeUnit time1{media::Microseconds(5)};
  media::Microseconds microseconds(5);
  media::TimeUnit time2 = media::TimeUnit(microseconds);
  EXPECT_EQ(time1, time2);

  i0 += media::TimeInterval(media::Microseconds(20), media::Microseconds(25));
  i0 += media::TimeInterval(media::Microseconds(40), media::Microseconds(60));
  i0 += media::TimeInterval(media::Microseconds(5), media::Microseconds(10));

  media::TimeIntervals i1;
  i1.Add(media::TimeInterval(media::Microseconds(16), media::Microseconds(27)));
  i1.Add(media::TimeInterval(media::Microseconds(7), media::Microseconds(15)));
  i1.Add(media::TimeInterval(media::Microseconds(53), media::Microseconds(57)));
  i1.Add(media::TimeInterval(media::Microseconds(45), media::Microseconds(50)));

  media::TimeIntervals i(i0 + i1);
  nsRefPtr<dom::TimeRanges> tr = new dom::TimeRanges();
  i.ToTimeRanges(tr);
  EXPECT_EQ(tr->Length(), i.Length());
  for (dom::TimeRanges::index_type index = 0; index < tr->Length(); index++) {
    ErrorResult rv;
    EXPECT_EQ(tr->Start(index, rv), i[index].mStart.ToSeconds());
    EXPECT_EQ(tr->Start(index, rv), i.Start(index).ToSeconds());
    EXPECT_EQ(tr->End(index, rv), i[index].mEnd.ToSeconds());
    EXPECT_EQ(tr->End(index, rv), i.End(index).ToSeconds());
  }

  i.Normalize();
  tr->Normalize();
  EXPECT_EQ(tr->Length(), i.Length());
  for (dom::TimeRanges::index_type index = 0; index < tr->Length(); index++) {
    ErrorResult rv;
    EXPECT_EQ(tr->Start(index, rv), i[index].mStart.ToSeconds());
    EXPECT_EQ(tr->Start(index, rv), i.Start(index).ToSeconds());
    EXPECT_EQ(tr->End(index, rv), i[index].mEnd.ToSeconds());
    EXPECT_EQ(tr->End(index, rv), i.End(index).ToSeconds());
  }
}

template<typename T>
class Foo
{
public:
  Foo()
    : mArg1(1)
    , mArg2(2)
    , mArg3(3)
  {}

  Foo(T a1, T a2, T a3)
    : mArg1(a1)
    , mArg2(a2)
    , mArg3(a3)
  {}

  Foo<T> operator+ (const Foo<T>& aOther) const
  {
    Foo<T> blah;
    blah.mArg1 += aOther.mArg1;
    blah.mArg2 += aOther.mArg2;
    blah.mArg3 += aOther.mArg3;
    return blah;
  }
  Foo<T> operator- (const Foo<T>& aOther) const
  {
    Foo<T> blah;
    blah.mArg1 -= aOther.mArg1;
    blah.mArg2 -= aOther.mArg2;
    blah.mArg3 -= aOther.mArg3;
    return blah;
  }
  bool operator< (const Foo<T>& aOther) const
  {
    return mArg1 < aOther.mArg1;
  }
  bool operator== (const Foo<T>& aOther) const
  {
    return mArg1 == aOther.mArg1;
  }
  bool operator<= (const Foo<T>& aOther) const
  {
    return mArg1 <= aOther.mArg1;
  }

private:
  int32_t mArg1;
  int32_t mArg2;
  int32_t mArg3;
};

TEST(IntervalSet, FooIntervalSet)
{
  media::Interval<Foo<int>> i(Foo<int>(), Foo<int>(4,5,6));
  media::IntervalSet<Foo<int>> is;
  is += i;
  is += i;
  is.Add(i);
  is = is + i;
  is = i + is;
  EXPECT_EQ(5u, is.Length());
  is.Normalize();
  EXPECT_EQ(1u, is.Length());
  EXPECT_EQ(Foo<int>(), is[0].mStart);
  EXPECT_EQ(Foo<int>(4,5,6), is[0].mEnd);
}
