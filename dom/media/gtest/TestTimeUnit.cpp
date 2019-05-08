/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "TimeUnits.h"
#include <algorithm>
#include <vector>

using namespace mozilla;
using namespace mozilla::media;

TEST(TimeUnit, Rounding)
{
  int64_t usecs = 66261715;
  double seconds = media::TimeUnit::FromMicroseconds(usecs).ToSeconds();
  EXPECT_EQ(media::TimeUnit::FromSeconds(seconds).ToMicroseconds(), usecs);

  seconds = 4.169470;
  usecs = 4169470;
  EXPECT_EQ(media::TimeUnit::FromSeconds(seconds).ToMicroseconds(), usecs);
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
