/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.8.2.11.js
   ECMA Section:       15.8.2.11 Math.max(x, y)
   Description:        return the smaller of the two arguments.
   special cases:
   - if x is NaN or y is NaN   return NaN
   - if x < y                  return x
   - if y > x                  return y
   - if x is +0 and y is +0    return +0
   - if x is +0 and y is -0    return -0
   - if x is -0 and y is +0    return -0
   - if x is -0 and y is -0    return -0
   Author:             christine@netscape.com
   Date:               7 july 1997
*/

var SECTION = "15.8.2.11";
var VERSION = "ECMA_1";
var TITLE   = "Math.max(x, y)";
var BUGNUMBER="76439";

startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "Math.max.length",
	      2,
	      Math.max.length );

new TestCase( SECTION,
	      "Math.max()",
	      -Infinity,
	      Math.max() );

new TestCase( SECTION,
	      "Math.max(void 0, 1)",
	      Number.NaN,
	      Math.max( void 0, 1 ) );

new TestCase( SECTION,
	      "Math.max(void 0, void 0)",
	      Number.NaN,
	      Math.max( void 0, void 0 ) );

new TestCase( SECTION,
	      "Math.max(null, 1)",
	      1,
	      Math.max( null, 1 ) );

new TestCase( SECTION,
	      "Math.max(-1, null)",
	      0,
	      Math.max( -1, null ) );

new TestCase( SECTION,
	      "Math.max(true, false)",
	      1,
	      Math.max(true,false) );

new TestCase( SECTION,
	      "Math.max('-99','99')",
	      99,
	      Math.max( "-99","99") );

new TestCase( SECTION,
	      "Math.max(NaN, Infinity)",
	      Number.NaN,
	      Math.max(Number.NaN,Number.POSITIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.max(NaN, 0)",
	      Number.NaN,
	      Math.max(Number.NaN, 0) );

new TestCase( SECTION,
	      "Math.max('a string', 0)",
	      Number.NaN,
	      Math.max("a string", 0) );

new TestCase( SECTION,
	      "Math.max(NaN, 1)",
	      Number.NaN,
	      Math.max(Number.NaN,1) );

new TestCase( SECTION,
	      "Math.max('a string',Infinity)",
	      Number.NaN,
	      Math.max("a string", Number.POSITIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.max(Infinity, NaN)",
	      Number.NaN,
	      Math.max( Number.POSITIVE_INFINITY, Number.NaN) );

new TestCase( SECTION,
	      "Math.max(NaN, NaN)",
	      Number.NaN,
	      Math.max(Number.NaN, Number.NaN) );

new TestCase( SECTION,
	      "Math.max(0,NaN)",
	      Number.NaN,
	      Math.max(0,Number.NaN) );

new TestCase( SECTION,
	      "Math.max(1, NaN)",
	      Number.NaN,
	      Math.max(1, Number.NaN) );

new TestCase( SECTION,
	      "Math.max(0,0)",
              0,
	      Math.max(0,0) );

new TestCase( SECTION,
	      "Math.max(0,-0)",
	      0,
	      Math.max(0,-0) );

new TestCase( SECTION,
	      "Math.max(-0,0)",
	      0,
	      Math.max(-0,0) );

new TestCase( SECTION,
	      "Math.max(-0,-0)",
	      -0,
	      Math.max(-0,-0) );

new TestCase( SECTION,
	      "Infinity/Math.max(-0,-0)",
	      -Infinity,
	      Infinity/Math.max(-0,-0) );

new TestCase( SECTION,
	      "Math.max(Infinity, Number.MAX_VALUE)", Number.POSITIVE_INFINITY,
	      Math.max(Number.POSITIVE_INFINITY, Number.MAX_VALUE) );

new TestCase( SECTION,
	      "Math.max(Infinity, Infinity)",
	      Number.POSITIVE_INFINITY,
	      Math.max(Number.POSITIVE_INFINITY,Number.POSITIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.max(-Infinity,-Infinity)",
	      Number.NEGATIVE_INFINITY,
	      Math.max(Number.NEGATIVE_INFINITY,Number.NEGATIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.max(1,.99999999999999)",
	      1,
	      Math.max(1,.99999999999999) );

new TestCase( SECTION,
	      "Math.max(-1,-.99999999999999)",
	      -.99999999999999,
	      Math.max(-1,-.99999999999999) );

test();
