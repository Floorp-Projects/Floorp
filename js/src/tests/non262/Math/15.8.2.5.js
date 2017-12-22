/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.8.2.5.js
   ECMA Section:       15.8.2.5 atan2( y, x )
   Description:

   Author:             christine@netscape.com
   Date:               7 july 1997

*/
var SECTION = "15.8.2.5";
var TITLE   = "Math.atan2(x,y)";
var BUGNUMBER="76111";

printBugNumber(BUGNUMBER);

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase(
	      "Math.atan2.length",
	      2,
	      Math.atan2.length );

new TestCase(
	      "Math.atan2(NaN, 0)",
	      Number.NaN,
	      Math.atan2(Number.NaN,0) );

new TestCase(
	      "Math.atan2(null, null)",
	      0,
	      Math.atan2(null, null) );

new TestCase(
	      "Math.atan2(void 0, void 0)",
	      Number.NaN,
	      Math.atan2(void 0, void 0) );

new TestCase(
	      "Math.atan2()",
	      Number.NaN,
	      Math.atan2() );

new TestCase(
	      "Math.atan2(0, NaN)",
	      Number.NaN,
	      Math.atan2(0,Number.NaN) );

new TestCase(
	      "Math.atan2(1, 0)",
	      Math.PI/2,
	      Math.atan2(1,0)          );

new TestCase(
	      "Math.atan2(1,-0)",
	      Math.PI/2,
	      Math.atan2(1,-0)         );

new TestCase(
	      "Math.atan2(0,0.001)",
	      0,
	      Math.atan2(0,0.001)      );

new TestCase(
	      "Math.atan2(0,0)",
	      0,
	      Math.atan2(0,0)          );

new TestCase(
	      "Math.atan2(0, -0)",
	      Math.PI,
	      Math.atan2(0,-0)         );

new TestCase(
	      "Math.atan2(0, -1)",
	      Math.PI,
	      Math.atan2(0, -1)        );

new TestCase(
	      "Math.atan2(-0, 1)",
	      -0,
	      Math.atan2(-0, 1)        );

new TestCase(
	      "Infinity/Math.atan2(-0, 1)",
	      -Infinity,
	      Infinity/Math.atan2(-0,1) );

new TestCase(
	      "Math.atan2(-0,	0)",
	      -0,
	      Math.atan2(-0,0)         );

new TestCase(
	      "Math.atan2(-0,	-0)",
	      -Math.PI,
	      Math.atan2(-0, -0)       );

new TestCase(
	      "Math.atan2(-0,	-1)",
	      -Math.PI,
	      Math.atan2(-0, -1)       );

new TestCase(
	      "Math.atan2(-1,	0)",
	      -Math.PI/2,
	      Math.atan2(-1, 0)        );

new TestCase(
	      "Math.atan2(-1,	-0)",
	      -Math.PI/2,
	      Math.atan2(-1, -0)       );

new TestCase(
	      "Math.atan2(1, Infinity)",
	      0,
	      Math.atan2(1, Number.POSITIVE_INFINITY) );

new TestCase(
	      "Math.atan2(1,-Infinity)", 
	      Math.PI,
	      Math.atan2(1, Number.NEGATIVE_INFINITY) );

new TestCase(
	      "Math.atan2(-1, Infinity)",
	      -0,
	      Math.atan2(-1,Number.POSITIVE_INFINITY) );

new TestCase(
	      "Infinity/Math.atan2(-1, Infinity)",
	      -Infinity, 
	      Infinity/Math.atan2(-1,Infinity) );

new TestCase(
	      "Math.atan2(-1,-Infinity)",
	      -Math.PI,
	      Math.atan2(-1,Number.NEGATIVE_INFINITY) );

new TestCase(
	      "Math.atan2(Infinity, 0)", 
	      Math.PI/2,
	      Math.atan2(Number.POSITIVE_INFINITY, 0) );

new TestCase(
	      "Math.atan2(Infinity, 1)", 
	      Math.PI/2,
	      Math.atan2(Number.POSITIVE_INFINITY, 1) );

new TestCase(
	      "Math.atan2(Infinity,-1)", 
	      Math.PI/2,
	      Math.atan2(Number.POSITIVE_INFINITY,-1) );

new TestCase(
	      "Math.atan2(Infinity,-0)", 
	      Math.PI/2,
	      Math.atan2(Number.POSITIVE_INFINITY,-0) );

new TestCase(
	      "Math.atan2(-Infinity, 0)",
	      -Math.PI/2,
	      Math.atan2(Number.NEGATIVE_INFINITY, 0) );

new TestCase(
	      "Math.atan2(-Infinity,-0)",
	      -Math.PI/2,
	      Math.atan2(Number.NEGATIVE_INFINITY,-0) );

new TestCase(
	      "Math.atan2(-Infinity, 1)",
	      -Math.PI/2,
	      Math.atan2(Number.NEGATIVE_INFINITY, 1) );

new TestCase(
	      "Math.atan2(-Infinity, -1)",
	      -Math.PI/2,
	      Math.atan2(Number.NEGATIVE_INFINITY,-1) );

new TestCase(
	      "Math.atan2(Infinity, Infinity)",
	      Math.PI/4,
	      Math.atan2(Number.POSITIVE_INFINITY, Number.POSITIVE_INFINITY) );

new TestCase(
	      "Math.atan2(Infinity, -Infinity)", 
	      3*Math.PI/4,
	      Math.atan2(Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY) );

new TestCase(
	      "Math.atan2(-Infinity, Infinity)", 
	      -Math.PI/4,
	      Math.atan2(Number.NEGATIVE_INFINITY, Number.POSITIVE_INFINITY) );

new TestCase(
	      "Math.atan2(-Infinity, -Infinity)",
	      -3*Math.PI/4,
	      Math.atan2(Number.NEGATIVE_INFINITY, Number.NEGATIVE_INFINITY) );

new TestCase(
	      "Math.atan2(-1, 1)",
	      -Math.PI/4,
	      Math.atan2( -1, 1) );

test();
