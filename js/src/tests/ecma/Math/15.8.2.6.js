/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.8.2.6.js
   ECMA Section:       15.8.2.6  Math.ceil(x)
   Description:        return the smallest number value that is not less than the
   argument and is equal to a mathematical integer.  if the
   number is already an integer, return the number itself.
   special cases:
   - if x is NaN       return NaN
   - if x = +0         return +0
   - if x = 0          return -0
   - if x = Infinity   return Infinity
   - if x = -Infinity  return -Infinity
   - if ( -1 < x < 0 ) return -0
   also:
   -   the value of Math.ceil(x) == -Math.ceil(-x)
   Author:             christine@netscape.com
   Date:               7 july 1997
*/
var SECTION = "15.8.2.6";
var TITLE   = "Math.ceil(x)";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase(
	      "Math.ceil.length",
	      1,
	      Math.ceil.length );

new TestCase(
	      "Math.ceil(NaN)",
	      Number.NaN,
	      Math.ceil(Number.NaN)   );

new TestCase(
	      "Math.ceil(null)",
	      0, 
	      Math.ceil(null) );

new TestCase(
	      "Math.ceil()",
	      Number.NaN,
	      Math.ceil() );

new TestCase(
	      "Math.ceil(void 0)",
	      Number.NaN,
	      Math.ceil(void 0) );

new TestCase(
	      "Math.ceil('0')",
	      0,
	      Math.ceil('0')            );

new TestCase(
	      "Math.ceil('-0')",
	      -0,
	      Math.ceil('-0')           );

new TestCase(
	      "Infinity/Math.ceil('0')",
	      Infinity,
	      Infinity/Math.ceil('0'));

new TestCase(
	      "Infinity/Math.ceil('-0')",
	      -Infinity,
	      Infinity/Math.ceil('-0'));

new TestCase(
	      "Math.ceil(0)",
	      0,
	      Math.ceil(0)            );

new TestCase(
	      "Math.ceil(-0)",
	      -0,
	      Math.ceil(-0)           );

new TestCase(
	      "Infinity/Math.ceil(0)",
	      Infinity,
	      Infinity/Math.ceil(0));

new TestCase(
	      "Infinity/Math.ceil(-0)",
	      -Infinity,
	      Infinity/Math.ceil(-0));


new TestCase(
	      "Math.ceil(Infinity)",
	      Number.POSITIVE_INFINITY,
	      Math.ceil(Number.POSITIVE_INFINITY) );

new TestCase(
	      "Math.ceil(-Infinity)",
	      Number.NEGATIVE_INFINITY,
	      Math.ceil(Number.NEGATIVE_INFINITY) );

new TestCase(
	      "Math.ceil(-Number.MIN_VALUE)",
	      -0,
	      Math.ceil(-Number.MIN_VALUE) );

new TestCase(
	      "Infinity/Math.ceil(-Number.MIN_VALUE)",
	      -Infinity,
	      Infinity/Math.ceil(-Number.MIN_VALUE) );

new TestCase(
	      "Math.ceil(1)",
	      1,
	      Math.ceil(1)   );

new TestCase(
	      "Math.ceil(-1)",
	      -1,
	      Math.ceil(-1)   );

new TestCase(
	      "Math.ceil(-0.9)",
	      -0,
	      Math.ceil(-0.9) );

new TestCase(
	      "Infinity/Math.ceil(-0.9)",
	      -Infinity,
	      Infinity/Math.ceil(-0.9) );

new TestCase(
	      "Math.ceil(0.9 )",
	      1,
	      Math.ceil( 0.9) );

new TestCase(
	      "Math.ceil(-1.1)",
	      -1,
	      Math.ceil( -1.1));

new TestCase(
	      "Math.ceil( 1.1)",
	      2,
	      Math.ceil(  1.1));

new TestCase(
	      "Math.ceil(Infinity)",
	      -Math.floor(-Infinity),
	      Math.ceil(Number.POSITIVE_INFINITY) );

new TestCase(
	      "Math.ceil(-Infinity)",
	      -Math.floor(Infinity),
	      Math.ceil(Number.NEGATIVE_INFINITY) );

new TestCase(
	      "Math.ceil(-Number.MIN_VALUE)",
	      -Math.floor(Number.MIN_VALUE),
	      Math.ceil(-Number.MIN_VALUE) );

new TestCase(
	      "Math.ceil(1)",
	      -Math.floor(-1),
	      Math.ceil(1)   );

new TestCase(
	      "Math.ceil(-1)",
	      -Math.floor(1),
	      Math.ceil(-1)   );

new TestCase(
	      "Math.ceil(-0.9)",
	      -Math.floor(0.9),
	      Math.ceil(-0.9) );

new TestCase(
	      "Math.ceil(0.9 )",
	      -Math.floor(-0.9),
	      Math.ceil( 0.9) );

new TestCase(
	      "Math.ceil(-1.1)",
	      -Math.floor(1.1),
	      Math.ceil( -1.1));

new TestCase(
	      "Math.ceil( 1.1)",
	      -Math.floor(-1.1),
	      Math.ceil(  1.1));

test();
