/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.1.13-1.js
   ECMA Section:       15.9.1.1 MakeDate(day, time)
   Description:

   The operator MakeDate calculates a number of milliseconds from its
   two arguments, which must be ECMAScript number values. This
   operator functions as follows:

   1. If day is not finite or time is not finite, return NaN.

   2. Compute day * msPerDay + time.

   3. Return Result(2).
*/

new TestCase( SECTION,
              "MakeDate(Number.POSITIVE_INFINITY, 0)",
              Number.NaN,
              MakeDate(Number.POSITIVE_INFINITY, 0));

new TestCase( SECTION,
              "MakeDate(Number.NEGATIVE_INFINITY, 0)",
              Number.NaN,
              MakeDate(Number.NEGATIVE_INFINITY, 0));

new TestCase( SECTION,
              "MakeDate(0, Number.POSITIVE_INFINITY)",
              Number.NaN,
              MakeDate(0, Number.POSITIVE_INFINITY));

new TestCase( SECTION,
              "MakeDate(0, Number.NEGATIVE_INFINITY)",
              Number.NaN,
              MakeDate(0, Number.NEGATIVE_INFINITY));

test();

