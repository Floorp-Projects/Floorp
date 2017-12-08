/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.8.2.9.js
   ECMA Section:       15.8.2.9  Math.floor(x)
   Description:        return the greatest number value that is not greater
   than the argument and is equal to a mathematical integer.
   if the number is already an integer, return the number
   itself.  special cases:
   - if x is NaN       return NaN
   - if x = +0         return +0
   - if x = -0          return -0
   - if x = Infinity   return Infinity
   - if x = -Infinity  return -Infinity
   - if ( -1 < x < 0 ) return -0
   also:
   -   the value of Math.floor(x) == -Math.ceil(-x)
   Author:             christine@netscape.com
   Date:               7 july 1997
*/

var SECTION = "15.8.2.9";
var TITLE   = "Math.floor(x)";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase(
	      "Math.floor.length",
	      1,
	      Math.floor.length );

new TestCase(
	      "Math.floor()",
	      Number.NaN,
	      Math.floor() );

new TestCase(
	      "Math.floor(void 0)",
	      Number.NaN,
	      Math.floor(void 0) );

new TestCase(
	      "Math.floor(null)",
	      0,
	      Math.floor(null) );

new TestCase(
	      "Math.floor(true)",
	      1,
	      Math.floor(true) );

new TestCase(
	      "Math.floor(false)",
	      0,
	      Math.floor(false) );

new TestCase(
	      "Math.floor('1.1')",
	      1,
	      Math.floor("1.1") );

new TestCase(
	      "Math.floor('-1.1')",
	      -2,
	      Math.floor("-1.1") );

new TestCase(
	      "Math.floor('0.1')",
	      0,
	      Math.floor("0.1") );

new TestCase(
	      "Math.floor('-0.1')",
	      -1,
	      Math.floor("-0.1") );

new TestCase(
	      "Math.floor(NaN)",
	      Number.NaN,
	      Math.floor(Number.NaN)  );

new TestCase(
	      "Math.floor(NaN)==-Math.ceil(-NaN)",
	      false,
	      Math.floor(Number.NaN) == -Math.ceil(-Number.NaN) );

new TestCase(
	      "Math.floor(0)",
	      0,
	      Math.floor(0)           );

new TestCase(
	      "Math.floor(0)==-Math.ceil(-0)",
	      true,
	      Math.floor(0) == -Math.ceil(-0) );

new TestCase(
	      "Math.floor(-0)",
	      -0,
	      Math.floor(-0)          );

new TestCase(
	      "Infinity/Math.floor(-0)",
	      -Infinity,
	      Infinity/Math.floor(-0)          );

new TestCase(
	      "Math.floor(-0)==-Math.ceil(0)",
	      true,
	      Math.floor(-0)== -Math.ceil(0) );

new TestCase(
	      "Math.floor(Infinity)",
	      Number.POSITIVE_INFINITY,
	      Math.floor(Number.POSITIVE_INFINITY) );

new TestCase(
	      "Math.floor(Infinity)==-Math.ceil(-Infinity)",
	      true,
	      Math.floor(Number.POSITIVE_INFINITY) == -Math.ceil(Number.NEGATIVE_INFINITY) );

new TestCase(
	      "Math.floor(-Infinity)",
	      Number.NEGATIVE_INFINITY,
	      Math.floor(Number.NEGATIVE_INFINITY) );

new TestCase(
	      "Math.floor(-Infinity)==-Math.ceil(Infinity)",
	      true,
	      Math.floor(Number.NEGATIVE_INFINITY) == -Math.ceil(Number.POSITIVE_INFINITY) );

new TestCase(
	      "Math.floor(0.0000001)",
	      0,
	      Math.floor(0.0000001) );

new TestCase(
	      "Math.floor(0.0000001)==-Math.ceil(0.0000001)", true,
	      Math.floor(0.0000001)==-Math.ceil(-0.0000001) );

new TestCase(
	      "Math.floor(-0.0000001)",
	      -1,
	      Math.floor(-0.0000001) );

new TestCase(
	      "Math.floor(0.0000001)==-Math.ceil(0.0000001)",
	      true,
	      Math.floor(-0.0000001)==-Math.ceil(0.0000001) );

test();
