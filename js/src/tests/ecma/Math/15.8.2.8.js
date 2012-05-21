/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.8.2.8.js
   ECMA Section:       15.8.2.8  Math.exp(x)
   Description:        return an approximation to the exponential function of
   the argument (e raised to the power of the argument)
   special cases:
   -   if x is NaN         return NaN
   -   if x is 0           return 1
   -   if x is -0          return 1
   -   if x is Infinity    return Infinity
   -   if x is -Infinity   return 0
   Author:             christine@netscape.com
   Date:               7 july 1997
*/


var SECTION = "15.8.2.8";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Math.exp(x)";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "Math.exp.length",
	      1,
	      Math.exp.length );

new TestCase( SECTION,
	      "Math.exp()",
	      Number.NaN,
	      Math.exp() );

new TestCase( SECTION,
	      "Math.exp(null)",
	      1,
	      Math.exp(null) );

new TestCase( SECTION,
	      "Math.exp(void 0)",
	      Number.NaN,
	      Math.exp(void 0) );

new TestCase( SECTION,
	      "Math.exp(1)",
	      Math.E,
	      Math.exp(1) );

new TestCase( SECTION,
	      "Math.exp(true)",
	      Math.E,
	      Math.exp(true) );

new TestCase( SECTION,
	      "Math.exp(false)",
	      1,
	      Math.exp(false) );

new TestCase( SECTION,
	      "Math.exp('1')",
	      Math.E,
	      Math.exp('1') );

new TestCase( SECTION,
	      "Math.exp('0')",
	      1,
	      Math.exp('0') );

new TestCase( SECTION,
	      "Math.exp(NaN)",
	      Number.NaN,
	      Math.exp(Number.NaN) );

new TestCase( SECTION,
	      "Math.exp(0)",
	      1,
	      Math.exp(0)          );

new TestCase( SECTION,
	      "Math.exp(-0)",
	      1,
	      Math.exp(-0)         );

new TestCase( SECTION,
	      "Math.exp(Infinity)",
	      Number.POSITIVE_INFINITY,
	      Math.exp(Number.POSITIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.exp(-Infinity)", 
	      0,
	      Math.exp(Number.NEGATIVE_INFINITY) );

test();
