/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestPoint.h"

#include "Point.h"

using namespace mozilla::gfx;

TestPoint::TestPoint()
{
  REGISTER_TEST(TestPoint, Addition);
  REGISTER_TEST(TestPoint, Subtraction);
}

void
TestPoint::Addition()
{
  Point a, b;
  a.x = 2;
  a.y = 2;
  b.x = 5;
  b.y = -5;

  a += b;

  VERIFY(a.x == 7.f);
  VERIFY(a.y == -3.f);
}

void
TestPoint::Subtraction()
{
  Point a, b;
  a.x = 2;
  a.y = 2;
  b.x = 5;
  b.y = -5;

  a -= b;

  VERIFY(a.x == -3.f);
  VERIFY(a.y == 7.f);
}
