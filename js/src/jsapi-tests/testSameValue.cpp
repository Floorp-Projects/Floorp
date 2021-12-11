/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/Equality.h"  // JS::SameValue
#include "jsapi-tests/tests.h"

BEGIN_TEST(testSameValue) {
  /*
   * NB: passing a double that fits in an integer jsval is API misuse.  As a
   * matter of defense in depth, however, JS::SameValue should return the
   * correct result comparing a positive-zero double to a negative-zero
   * double, and this is believed to be the only way to make such a
   * comparison possible.
   */
  JS::RootedValue v1(cx, JS::DoubleValue(0.0));
  JS::RootedValue v2(cx, JS::DoubleValue(-0.0));
  bool same;
  CHECK(JS::SameValue(cx, v1, v2, &same));
  CHECK(!same);
  return true;
}
END_TEST(testSameValue)
