/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mp4_demuxer/Interval.h"

using namespace mp4_demuxer;
using namespace mozilla;

TEST(Interval, Length)
{
  Interval<int> i(15, 25);
  EXPECT_EQ(10, i.Length());
}

TEST(Interval, Intersection)
{
  Interval<int> i0(10, 20);
  Interval<int> i1(15, 25);
  Interval<int> i = i0.Intersection(i1);
  EXPECT_EQ(15, i.start);
  EXPECT_EQ(20, i.end);
}

TEST(Interval, Equals)
{
  Interval<int> i0(10, 20);
  Interval<int> i1(10, 20);
  EXPECT_EQ(i0, i1);

  Interval<int> i2(5, 20);
  EXPECT_NE(i0, i2);

  Interval<int> i3(10, 15);
  EXPECT_NE(i0, i2);
}

TEST(Interval, IntersectionVector)
{
  nsTArray<Interval<int>> i0;
  i0.AppendElement(Interval<int>(5, 10));
  i0.AppendElement(Interval<int>(20, 25));
  i0.AppendElement(Interval<int>(40, 60));

  nsTArray<Interval<int>> i1;
  i1.AppendElement(Interval<int>(7, 15));
  i1.AppendElement(Interval<int>(16, 27));
  i1.AppendElement(Interval<int>(45, 50));
  i1.AppendElement(Interval<int>(53, 57));

  nsTArray<Interval<int>> i;
  Interval<int>::Intersection(i0, i1, &i);

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

TEST(Interval, Normalize)
{
  nsTArray<Interval<int>> i;
  i.AppendElement(Interval<int>(20, 30));
  i.AppendElement(Interval<int>(1, 8));
  i.AppendElement(Interval<int>(5, 10));
  i.AppendElement(Interval<int>(2, 7));

  nsTArray<Interval<int>> o;
  Interval<int>::Normalize(i, &o);

  EXPECT_EQ(2u, o.Length());

  EXPECT_EQ(1, o[0].start);
  EXPECT_EQ(10, o[0].end);

  EXPECT_EQ(20, o[1].start);
  EXPECT_EQ(30, o[1].end);
}
