/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.8.2.17.js
   ECMA Section:       15.8.2.17  Math.sqrt(x)
   Description:        return an approximation to the squareroot of the argument.
   special cases:
   -   if x is NaN         return NaN
   -   if x < 0            return NaN
   -   if x == 0           return 0
   -   if x == -0          return -0
   -   if x == Infinity    return Infinity
   Author:             christine@netscape.com
   Date:               7 july 1997
*/

var SECTION = "15.8.2.17";
var TITLE   = "Math.sqrt(x)";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase(
	      "Math.sqrt.length",
	      1,
	      Math.sqrt.length );

new TestCase(
	      "Math.sqrt()",
	      Number.NaN,
	      Math.sqrt() );

new TestCase(
	      "Math.sqrt(void 0)",
	      Number.NaN,
	      Math.sqrt(void 0) );

new TestCase(
	      "Math.sqrt(null)",
	      0,
	      Math.sqrt(null) );

new TestCase(
	      "Math.sqrt(true)",
	      1,
	      Math.sqrt(1) );

new TestCase(
	      "Math.sqrt(false)",
	      0,
	      Math.sqrt(false) );

new TestCase(
	      "Math.sqrt('225')",
	      15,
	      Math.sqrt('225') );

new TestCase(
	      "Math.sqrt(NaN)",
	      Number.NaN,
	      Math.sqrt(Number.NaN) );

new TestCase(
	      "Math.sqrt(-Infinity)",
	      Number.NaN,
	      Math.sqrt(Number.NEGATIVE_INFINITY));

new TestCase(
	      "Math.sqrt(-1)",
	      Number.NaN,
	      Math.sqrt(-1));

new TestCase(
	      "Math.sqrt(-0.5)",
	      Number.NaN,
	      Math.sqrt(-0.5));

new TestCase(
	      "Math.sqrt(0)",
	      0,
	      Math.sqrt(0));

new TestCase(
	      "Math.sqrt(-0)",
	      -0,
	      Math.sqrt(-0));

new TestCase(
	      "Infinity/Math.sqrt(-0)",
	      -Infinity,
	      Infinity/Math.sqrt(-0) );

new TestCase(
	      "Math.sqrt(Infinity)",
	      Number.POSITIVE_INFINITY,
	      Math.sqrt(Number.POSITIVE_INFINITY));

new TestCase(
	      "Math.sqrt(1)",
	      1,
	      Math.sqrt(1));

new TestCase(
	      "Math.sqrt(2)",
	      Math.SQRT2,
	      Math.sqrt(2));

new TestCase(
	      "Math.sqrt(0.5)",
	      Math.SQRT1_2,
	      Math.sqrt(0.5));

new TestCase(
	      "Math.sqrt(4)",
	      2,
	      Math.sqrt(4));

new TestCase(
	      "Math.sqrt(9)",
	      3,
	      Math.sqrt(9));

new TestCase(
	      "Math.sqrt(16)",
	      4,
	      Math.sqrt(16));

new TestCase(
	      "Math.sqrt(25)",
	      5,
	      Math.sqrt(25));

new TestCase(
	      "Math.sqrt(36)",
	      6,
	      Math.sqrt(36));

new TestCase(
	      "Math.sqrt(49)",
	      7,
	      Math.sqrt(49));

new TestCase(
	      "Math.sqrt(64)",
	      8,
	      Math.sqrt(64));

new TestCase(
	      "Math.sqrt(256)",
	      16,
	      Math.sqrt(256));

new TestCase(
	      "Math.sqrt(10000)",
	      100,
	      Math.sqrt(10000));

new TestCase(
	      "Math.sqrt(65536)",
	      256,
	      Math.sqrt(65536));

new TestCase(
	      "Math.sqrt(0.09)",
	      0.3,
	      Math.sqrt(0.09));

new TestCase(
	      "Math.sqrt(0.01)",
	      0.1,
	      Math.sqrt(0.01));

new TestCase(
	      "Math.sqrt(0.00000001)",
	      0.0001,
	      Math.sqrt(0.00000001));

test();
