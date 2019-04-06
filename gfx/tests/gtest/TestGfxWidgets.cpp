/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "GfxDriverInfo.h"
#include "nsVersionComparator.h"

using namespace mozilla::widget;

TEST(GfxWidgets, Split)
{
  char aStr[8], bStr[8], cStr[8], dStr[8];

  ASSERT_TRUE(SplitDriverVersion("33.4.3.22", aStr, bStr, cStr, dStr));
  ASSERT_TRUE(atoi(aStr) == 33 && atoi(bStr) == 4 && atoi(cStr) == 3 &&
              atoi(dStr) == 22);

  ASSERT_TRUE(SplitDriverVersion("28.74.0.0", aStr, bStr, cStr, dStr));
  ASSERT_TRUE(atoi(aStr) == 28 && atoi(bStr) == 74 && atoi(cStr) == 0 &&
              atoi(dStr) == 0);

  ASSERT_TRUE(SplitDriverVersion("132.0.0.0", aStr, bStr, cStr, dStr));
  ASSERT_TRUE(atoi(aStr) == 132 && atoi(bStr) == 0 && atoi(cStr) == 0 &&
              atoi(dStr) == 0);

  ASSERT_TRUE(SplitDriverVersion("2.3.0.0", aStr, bStr, cStr, dStr));
  ASSERT_TRUE(atoi(aStr) == 2 && atoi(bStr) == 3 && atoi(cStr) == 0 &&
              atoi(dStr) == 0);

  ASSERT_TRUE(SplitDriverVersion("25.4.0.8", aStr, bStr, cStr, dStr));
  ASSERT_TRUE(atoi(aStr) == 25 && atoi(bStr) == 4 && atoi(cStr) == 0 &&
              atoi(dStr) == 8);

  ASSERT_TRUE(SplitDriverVersion("424.143.84437.3", aStr, bStr, cStr, dStr));
  ASSERT_TRUE(atoi(aStr) == 424 && atoi(bStr) == 143 && atoi(cStr) == 8443 &&
              atoi(dStr) == 3);

  ASSERT_FALSE(SplitDriverVersion("25.4.0.8.", aStr, bStr, cStr, dStr));
  ASSERT_TRUE(atoi(aStr) == 25 && atoi(bStr) == 4 && atoi(cStr) == 0 &&
              atoi(dStr) == 8);

  ASSERT_TRUE(SplitDriverVersion("424.143.8.3143243", aStr, bStr, cStr, dStr));
  ASSERT_TRUE(atoi(aStr) == 424 && atoi(bStr) == 143 && atoi(cStr) == 8 &&
              atoi(dStr) == 3143);

  ASSERT_FALSE(SplitDriverVersion("25.4.0.8..", aStr, bStr, cStr, dStr));
  ASSERT_TRUE(atoi(aStr) == 25 && atoi(bStr) == 4 && atoi(cStr) == 0 &&
              atoi(dStr) == 8);

  ASSERT_FALSE(
      SplitDriverVersion("424.143.8.3143243.", aStr, bStr, cStr, dStr));
  ASSERT_TRUE(atoi(aStr) == 424 && atoi(bStr) == 143 && atoi(cStr) == 8 &&
              atoi(dStr) == 3143);

  ASSERT_FALSE(SplitDriverVersion("25.4.0.8.13", aStr, bStr, cStr, dStr));
  ASSERT_TRUE(atoi(aStr) == 25 && atoi(bStr) == 4 && atoi(cStr) == 0 &&
              atoi(dStr) == 8);

  ASSERT_FALSE(SplitDriverVersion("4.1.8.13.24.35", aStr, bStr, cStr, dStr));
  ASSERT_TRUE(atoi(aStr) == 4 && atoi(bStr) == 1 && atoi(cStr) == 8 &&
              atoi(dStr) == 13);

  ASSERT_TRUE(SplitDriverVersion("28...74", aStr, bStr, cStr, dStr));
  ASSERT_TRUE(atoi(aStr) == 28 && atoi(bStr) == 0 && atoi(cStr) == 0 &&
              atoi(dStr) == 74);

  ASSERT_FALSE(SplitDriverVersion("4.1.8.13.24.35", aStr, bStr, cStr, dStr));
  ASSERT_TRUE(atoi(aStr) == 4 && atoi(bStr) == 1 && atoi(cStr) == 8 &&
              atoi(dStr) == 13);

  ASSERT_TRUE(SplitDriverVersion("35..42.0", aStr, bStr, cStr, dStr));
  ASSERT_TRUE(atoi(aStr) == 35 && atoi(bStr) == 0 && atoi(cStr) == 42 &&
              atoi(dStr) == 0);
}

TEST(GfxWidgets, Versioning)
{
  ASSERT_TRUE(mozilla::Version("0") < mozilla::Version("41.0a1"));
  ASSERT_TRUE(mozilla::Version("39.0.5b7") < mozilla::Version("41.0a1"));
  ASSERT_TRUE(mozilla::Version("18.0.5b7") < mozilla::Version("18.2"));
  ASSERT_TRUE(mozilla::Version("30.0.5b7") < mozilla::Version("41.0b9"));
  ASSERT_TRUE(mozilla::Version("100") > mozilla::Version("43.0a1"));
  ASSERT_FALSE(mozilla::Version("42.0") < mozilla::Version("42.0"));
  ASSERT_TRUE(mozilla::Version("42.0b2") < mozilla::Version("42.0"));
  ASSERT_TRUE(mozilla::Version("42.0b2") < mozilla::Version("42"));
  ASSERT_TRUE(mozilla::Version("42.0b2") < mozilla::Version("43.0a1"));
  ASSERT_TRUE(mozilla::Version("42") < mozilla::Version("43.0a1"));
  ASSERT_TRUE(mozilla::Version("42.0") < mozilla::Version("43.0a1"));
  ASSERT_TRUE(mozilla::Version("42.0.5") < mozilla::Version("43.0a1"));
  ASSERT_TRUE(mozilla::Version("42.1") < mozilla::Version("43.0a1"));
  ASSERT_TRUE(mozilla::Version("42.0a1") < mozilla::Version("42"));
  ASSERT_TRUE(mozilla::Version("42.0a1") < mozilla::Version("42.0.5"));
  ASSERT_TRUE(mozilla::Version("42.0b7") < mozilla::Version("42.0.5"));
  ASSERT_TRUE(mozilla::Version("") == mozilla::Version("0"));
  ASSERT_TRUE(mozilla::Version("1b1b") == mozilla::Version("1b1b"));
  ASSERT_TRUE(mozilla::Version("1b1b") < mozilla::Version("1b1c"));
  ASSERT_TRUE(mozilla::Version("1b1b") < mozilla::Version("1b1d"));
  ASSERT_TRUE(mozilla::Version("1b1c") > mozilla::Version("1b1b"));
  ASSERT_TRUE(mozilla::Version("1b1d") > mozilla::Version("1b1b"));

  // Note these two; one would expect for 42.0b1 and 42b1 to compare the
  // same, but they do not.  If this ever changes, we want to know, so
  // leave the test here to fail.
  ASSERT_TRUE(mozilla::Version("42.0a1") < mozilla::Version("42.0b2"));
  ASSERT_FALSE(mozilla::Version("42.0a1") < mozilla::Version("42b2"));
}
