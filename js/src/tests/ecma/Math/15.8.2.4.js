/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.8.2.4.js
   ECMA Section:       15.8.2.4 atan( x )
   Description:        return an approximation to the arc tangent of the
   argument.  the result is expressed in radians and
   range is from -PI/2 to +PI/2.  special cases:
   - if x is NaN,  the result is NaN
   - if x == +0,   the result is +0
   - if x == -0,   the result is -0
   - if x == +Infinity,    the result is approximately +PI/2
   - if x == -Infinity,    the result is approximately -PI/2
   Author:             christine@netscape.com
   Date:               7 july 1997

*/

var SECTION = "15.8.2.4";
var VERSION = "ECMA_1";
var TITLE   = "Math.atan()";
var BUGNUMBER="77391";

startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "Math.atan.length",
	      1,
	      Math.atan.length );

new TestCase( SECTION,
	      "Math.atan()",
	      Number.NaN,
	      Math.atan() );

new TestCase( SECTION,
	      "Math.atan(void 0)",
	      Number.NaN,
	      Math.atan(void 0) );

new TestCase( SECTION,
	      "Math.atan(null)",
	      0,
	      Math.atan(null) );

new TestCase( SECTION,
	      "Math.atan(NaN)",
	      Number.NaN,
	      Math.atan(Number.NaN) );

new TestCase( SECTION,
	      "Math.atan('a string')",
	      Number.NaN,
	      Math.atan("a string") );

new TestCase( SECTION,
	      "Math.atan('0')",
	      0,
	      Math.atan('0') );

new TestCase( SECTION,
	      "Math.atan('1')",
	      Math.PI/4,
	      Math.atan('1') );

new TestCase( SECTION,
	      "Math.atan('-1')",
	      -Math.PI/4,
	      Math.atan('-1') );

new TestCase( SECTION,
	      "Math.atan('Infinity)",
	      Math.PI/2,
	      Math.atan('Infinity') );

new TestCase( SECTION,
	      "Math.atan('-Infinity)",
	      -Math.PI/2,
	      Math.atan('-Infinity') );

new TestCase( SECTION,
	      "Math.atan(0)",
	      0,
	      Math.atan(0)          );

new TestCase( SECTION,
	      "Math.atan(-0)",	
	      -0,
	      Math.atan(-0)         );

new TestCase( SECTION,
	      "Infinity/Math.atan(-0)",
	      -Infinity,
	      Infinity/Math.atan(-0) );

new TestCase( SECTION,
	      "Math.atan(Infinity)",
	      Math.PI/2,
	      Math.atan(Number.POSITIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.atan(-Infinity)",
	      -Math.PI/2,
	      Math.atan(Number.NEGATIVE_INFINITY) );

new TestCase( SECTION,
	      "Math.atan(1)",
	      Math.PI/4,
	      Math.atan(1)          );

new TestCase( SECTION,
	      "Math.atan(-1)",
	      -Math.PI/4,
	      Math.atan(-1)         );

test();
