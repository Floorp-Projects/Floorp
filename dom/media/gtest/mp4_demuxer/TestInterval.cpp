/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "MP4Interval.h"

using mozilla::MP4Interval;

TEST(MP4Interval, Length)
{
  MP4Interval<int> i(15, 25);
  EXPECT_EQ(10, i.Length());
}

TEST(MP4Interval, Intersection)
{
  MP4Interval<int> i0(10, 20);
  MP4Interval<int> i1(15, 25);
  MP4Interval<int> i = i0.Intersection(i1);
  EXPECT_EQ(15, i.start);
  EXPECT_EQ(20, i.end);
}

TEST(MP4Interval, Equals)
{
  MP4Interval<int> i0(10, 20);
  MP4Interval<int> i1(10, 20);
  EXPECT_EQ(i0, i1);

  MP4Interval<int> i2(5, 20);
  EXPECT_NE(i0, i2);

  MP4Interval<int> i3(10, 15);
  EXPECT_NE(i0, i2);
}

TEST(MP4Interval, IntersectionVector)
{
  nsTArray<MP4Interval<int>> i0;
  i0.AppendElement(MP4Interval<int>(5, 10));
  i0.AppendElement(MP4Interval<int>(20, 25));
  i0.AppendElement(MP4Interval<int>(40, 60));

  nsTArray<MP4Interval<int>> i1;
  i1.AppendElement(MP4Interval<int>(7, 15));
  i1.AppendElement(MP4Interval<int>(16, 27));
  i1.AppendElement(MP4Interval<int>(45, 50));
  i1.AppendElement(MP4Interval<int>(53, 57));

  nsTArray<MP4Interval<int>> i;
  MP4Interval<int>::Intersection(i0, i1, &i);

  EXPECT_EQ(4u, i.Length());

  EXPECT_EQ(7, i[0].start);
  EXPECT_EQ(10, i[0].end);

  EXPECT_EQ(20, i[1].start);
  EXPECT_EQ(25, i[1].end);

  EXPECT_EQ(45, i[2].start);
  EXPECT_EQ(50, i[2].end);

  EXPECT_EQ(53, i[3].start);
  EXPECT_EQ(57, i[3].end);
}

TEST(MP4Interval, Normalize)
{
  nsTArray<MP4Interval<int>> i;
  i.AppendElement(MP4Interval<int>(20, 30));
  i.AppendElement(MP4Interval<int>(1, 8));
  i.AppendElement(MP4Interval<int>(5, 10));
  i.AppendElement(MP4Interval<int>(2, 7));

  nsTArray<MP4Interval<int>> o;
  MP4Interval<int>::Normalize(i, &o);

  EXPECT_EQ(2u, o.Length());

  EXPECT_EQ(1, o[0].start);
  EXPECT_EQ(10, o[0].end);

  EXPECT_EQ(20, o[1].start);
  EXPECT_EQ(30, o[1].end);
}
