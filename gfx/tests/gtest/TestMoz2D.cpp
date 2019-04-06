/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "TestBase.h"
#include "TestPoint.h"
#include "TestScaling.h"
#include "TestBugs.h"

TEST(Moz2D, Bugs)
{
  TestBugs* test = new TestBugs();
  int failures = 0;
  test->RunTests(&failures);
  delete test;

  ASSERT_EQ(failures, 0);
}

TEST(Moz2D, Point)
{
  TestBase* test = new TestPoint();
  int failures = 0;
  test->RunTests(&failures);
  delete test;

  ASSERT_EQ(failures, 0);
}

TEST(Moz2D, Scaling)
{
  TestBase* test = new TestScaling();
  int failures = 0;
  test->RunTests(&failures);
  delete test;

  ASSERT_EQ(failures, 0);
}
