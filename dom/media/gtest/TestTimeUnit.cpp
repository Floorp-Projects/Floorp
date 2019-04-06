/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "TimeUnits.h"
#include <algorithm>
#include <vector>

using namespace mozilla;

TEST(TimeUnit, Rounding)
{
  int64_t usecs = 66261715;
  double seconds = media::TimeUnit::FromMicroseconds(usecs).ToSeconds();
  EXPECT_EQ(media::TimeUnit::FromSeconds(seconds).ToMicroseconds(), usecs);

  seconds = 4.169470;
  usecs = 4169470;
  EXPECT_EQ(media::TimeUnit::FromSeconds(seconds).ToMicroseconds(), usecs);
}
