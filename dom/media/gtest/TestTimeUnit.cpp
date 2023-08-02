/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include <algorithm>
#include <vector>

#include "TimeUnits.h"

using namespace mozilla;
using namespace mozilla::media;
using TimeUnit = mozilla::media::TimeUnit;

TEST(TimeUnit, BasicArithmetic)
{
  const TimeUnit a(1000, 44100);
  {
    TimeUnit b = a * 10;
    EXPECT_EQ(b.mBase, 44100);
    EXPECT_EQ(b.mTicks.value(), a.mTicks.value() * 10);
    EXPECT_EQ(a * 10, b);
  }
  {
    TimeUnit b = a / 10;
    EXPECT_EQ(b.mBase, 44100);
    EXPECT_EQ(b.mTicks.value(), a.mTicks.value() / 10);
    EXPECT_EQ(a / 10, b);
  }
  {
    TimeUnit b = TimeUnit(10, 44100);
    b += a;
    EXPECT_EQ(b.mBase, 44100);
    EXPECT_EQ(b.mTicks.value(), a.mTicks.value() + 10);
    EXPECT_EQ(b - a, TimeUnit(10, 44100));
  }
  {
    TimeUnit b = TimeUnit(1010, 44100);
    b -= a;  // now 10
    EXPECT_EQ(b.mBase, 44100);
    EXPECT_EQ(b.mTicks.value(), 10);
    EXPECT_EQ(a + b, TimeUnit(1010, 44100));
  }
  {
    TimeUnit b = TimeUnit(4010, 44100);
    TimeUnit c = b % a;  // now 10
    EXPECT_EQ(c.mBase, 44100);
    EXPECT_EQ(c.mTicks.value(), 10);
  }
  {
    // Adding 6s in nanoseconds (e.g. coming from script) to a typical number
    // from an mp4, 9001 in base 90000
    TimeUnit b = TimeUnit(6000000000, 1000000000);
    TimeUnit c = TimeUnit(9001, 90000);
    TimeUnit d = c + b;
    EXPECT_EQ(d.mBase, 90000);
    EXPECT_EQ(d.mTicks.value(), 549001);
  }
  {
    // Subtracting 9001 in base 9000 from 6s in nanoseconds (e.g. coming from
    // script), converting to back to base 9000.
    TimeUnit b = TimeUnit(6000000000, 1000000000);
    TimeUnit c = TimeUnit(9001, 90000);
    TimeUnit d = (b - c).ToBase(90000);
    EXPECT_EQ(d.mBase, 90000);
    EXPECT_EQ(d.mTicks.value(), 530999);
  }
}

TEST(TimeUnit, Base)
{
  {
    TimeUnit a = TimeUnit::FromSeconds(1);
    EXPECT_EQ(a.mTicks.value(), 1000000);
    EXPECT_EQ(a.mBase, 1000000);
  }
  {
    TimeUnit a = TimeUnit::FromMicroseconds(44100000000);
    EXPECT_EQ(a.mTicks.value(), 44100000000);
    EXPECT_EQ(a.mBase, 1000000);
  }
  {
    TimeUnit a = TimeUnit::FromSeconds(6.0);
    EXPECT_EQ(a.mTicks.value(), 6000000);
    EXPECT_EQ(a.mBase, 1000000);
    double error;
    TimeUnit b = a.ToBase(90000, error);
    EXPECT_EQ(error, 0);
    EXPECT_EQ(b.mTicks.value(), 540000);
    EXPECT_EQ(b.mBase, 90000);
  }
}

TEST(TimeUnit, Rounding)
{
  int64_t usecs = 662617;
  double seconds = TimeUnit::FromMicroseconds(usecs).ToSeconds();
  TimeUnit fromSeconds = TimeUnit::FromSeconds(seconds);
  EXPECT_EQ(fromSeconds.mTicks.value(), usecs);
  // TimeUnit base is microseconds if not explicitly passed.
  EXPECT_EQ(fromSeconds.mBase, 1000000);
  EXPECT_EQ(fromSeconds.ToMicroseconds(), usecs);

  seconds = 4.169470123;
  int64_t nsecs = 4169470123;
  EXPECT_EQ(TimeUnit::FromSeconds(seconds, 1e9).ToNanoseconds(), nsecs);
  EXPECT_EQ(TimeUnit::FromSeconds(seconds, 1e9).ToMicroseconds(), nsecs / 1000);

  seconds = 2312312.16947012;
  nsecs = 2312312169470120;
  EXPECT_EQ(TimeUnit::FromSeconds(seconds, 1e9).ToNanoseconds(), nsecs);
  EXPECT_EQ(TimeUnit::FromSeconds(seconds, 1e9).ToMicroseconds(), nsecs / 1000);

  seconds = 2312312.169470123;
  nsecs = 2312312169470123;
  // A double doesn't have enough precision to roundtrip this time value
  // correctly in this base, but the number of microseconds is still correct.
  // This value is about 142.5 days however.
  // This particular calculation results in exactly 1ns of difference after
  // roundtrip. Enable this after remoing the MOZ_CRASH in TimeUnit::FromSeconds
  // EXPECT_EQ(TimeUnit::FromSeconds(seconds, 1e9).ToNanoseconds() - nsecs, 1);
  EXPECT_EQ(TimeUnit::FromSeconds(seconds, 1e9).ToMicroseconds(), nsecs / 1000);
}

TEST(TimeUnit, Comparisons)
{
  TimeUnit a(0, 1e9);
  TimeUnit b(1, 1e9);
  TimeUnit c(1, 1e6);

  EXPECT_GE(b, a);
  EXPECT_GE(c, a);
  EXPECT_GE(c, b);

  EXPECT_GT(b, a);
  EXPECT_GT(c, a);
  EXPECT_GT(c, b);

  EXPECT_LE(a, b);
  EXPECT_LE(a, c);
  EXPECT_LE(b, c);

  EXPECT_LT(a, b);
  EXPECT_LT(a, c);
  EXPECT_LT(b, c);

  // Equivalence of zero regardless of the base
  TimeUnit d(0, 1);
  TimeUnit e(0, 1000);
  EXPECT_EQ(a, d);
  EXPECT_EQ(a, e);

  // Equivalence of time accross bases
  TimeUnit f(1000, 1e9);
  TimeUnit g(1, 1e6);
  EXPECT_EQ(f, g);

  // Comparisons with infinity, same base
  TimeUnit h = TimeUnit::FromInfinity();
  TimeUnit i = TimeUnit::Zero();
  EXPECT_LE(i, h);
  EXPECT_LT(i, h);
  EXPECT_GE(h, i);
  EXPECT_GT(h, i);

  // Comparisons with infinity, different base
  TimeUnit j = TimeUnit::FromInfinity();
  TimeUnit k = TimeUnit::Zero(1000000);
  EXPECT_LE(k, j);
  EXPECT_LT(k, j);
  EXPECT_GE(j, k);
  EXPECT_GT(j, k);

  // Comparison of very big numbers, different base that have a gcd that makes
  // it easy to reduce, to test the fraction reduction code
  TimeUnit l = TimeUnit(123123120000000, 1000000000);
  TimeUnit m = TimeUnit(123123120000000, 1000);
  EXPECT_LE(l, m);
  EXPECT_LT(l, m);
  EXPECT_GE(m, l);
  EXPECT_GT(m, l);

  // Comparison of very big numbers, different base that are co-prime: worst
  // cast scenario.
  TimeUnit n = TimeUnit(123123123123123, 1000000000);
  TimeUnit o = TimeUnit(123123123123123, 1000000001);
  EXPECT_LE(o, n);
  EXPECT_LT(o, n);
  EXPECT_GE(n, o);
  EXPECT_GT(n, o);

  // Values taken from a real website (this is about 53 years, Date.now() in
  // 2023).
  TimeUnit leftBound(74332508253360, 44100);
  TimeUnit rightBound(74332508297392, 44100);
  TimeUnit fuzz(250000, 1000000);
  TimeUnit time(1685544404790205, 1000000);

  EXPECT_LT(leftBound - fuzz, time);
  EXPECT_GT(time, leftBound - fuzz);
  EXPECT_GE(rightBound + fuzz, time);
  EXPECT_LT(time, rightBound + fuzz);

  TimeUnit zero = TimeUnit::Zero();  // default base 1e6
  TimeUnit datenow(
      151737439364679,
      90000);  // Also from `Date.now()` in a common base for an mp4
  EXPECT_NE(zero, datenow);
}

TEST(TimeUnit, InfinityMath)
{
  // Operator plus/minus uses floating point behaviour for positive and
  // negative infinity values, i.e.:
  //  posInf + posInf	= inf
  //  posInf + negInf	= -nan
  //  posInf + finite = inf
  //  posInf - posInf	= -nan
  //  posInf - negInf	= inf
  //  posInf - finite = inf
  //  negInf + negInf	= -inf
  //  negInf + posInf	= -nan
  //  negInf + finite = -inf
  //  negInf - negInf	= -nan
  //  negInf - posInf	= -inf
  //  negInf - finite = -inf
  //  finite + posInf = inf
  //  finite - posInf = -inf
  //  finite + negInf = -inf
  //  finite - negInf = inf

  const TimeUnit posInf = TimeUnit::FromInfinity();
  EXPECT_EQ(TimeUnit::FromSeconds(mozilla::PositiveInfinity<double>()), posInf);

  const TimeUnit negInf = TimeUnit::FromNegativeInfinity();
  EXPECT_EQ(TimeUnit::FromSeconds(mozilla::NegativeInfinity<double>()), negInf);

  EXPECT_EQ(posInf + posInf, posInf);
  EXPECT_FALSE((posInf + negInf).IsValid());
  EXPECT_FALSE((posInf - posInf).IsValid());
  EXPECT_EQ(posInf - negInf, posInf);
  EXPECT_EQ(negInf + negInf, negInf);
  EXPECT_FALSE((negInf + posInf).IsValid());
  EXPECT_FALSE((negInf - negInf).IsValid());
  EXPECT_EQ(negInf - posInf, negInf);

  const TimeUnit finite = TimeUnit::FromSeconds(42.0);
  EXPECT_EQ(posInf - finite, posInf);
  EXPECT_EQ(posInf + finite, posInf);
  EXPECT_EQ(negInf - finite, negInf);
  EXPECT_EQ(negInf + finite, negInf);

  EXPECT_EQ(finite + posInf, posInf);
  EXPECT_EQ(finite - posInf, negInf);
  EXPECT_EQ(finite + negInf, negInf);
  EXPECT_EQ(finite - negInf, posInf);
}

TEST(TimeUnit, BaseConversion)
{
  const int64_t packetSize = 1024;  // typical for AAC
  int64_t sampleRates[] = {16000, 44100, 48000, 88200, 96000};
  const double hnsPerSeconds = 10000000.;
  for (auto sampleRate : sampleRates) {
    int64_t frameCount = 0;
    TimeUnit pts;
    do {
      // Compute a time in hundreds of nanoseconds based of frame count, typical
      // on Windows platform, checking that it round trips properly.
      int64_t hns = AssertedCast<int64_t>(
          std::round(hnsPerSeconds * static_cast<double>(frameCount) /
                     static_cast<double>(sampleRate)));
      pts = TimeUnit::FromHns(hns, sampleRate);
      EXPECT_EQ(
          AssertedCast<int64_t>(std::round(pts.ToSeconds() * hnsPerSeconds)),
          hns);
      frameCount += packetSize;
    } while (pts.ToSeconds() < 36000);
  }
}

TEST(TimeUnit, MinimumRoundingError)
{
  TimeUnit a(448, 48000);  // ≈9333 us
  TimeUnit b(1, 1000000);  // 1 us
  TimeUnit rv = a - b;     // should close to 9332 us as much as possible
  EXPECT_EQ(rv.mTicks.value(), 448);  // ≈9333 us is closer to 9332 us
  EXPECT_EQ(rv.mBase, 48000);

  TimeUnit c(11, 1000000);  // 11 us
  rv = a - c;               // should close to 9322 as much as possible
  EXPECT_EQ(rv.mTicks.value(), 447);  // ≈9312 us is closer to 9322 us
  EXPECT_EQ(rv.mBase, 48000);
}
