/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.8.2.13.js
   ECMA Section:       15.8.2.13 Math.pow(x, y)
   Description:        return an approximation to the result of x
   to the power of y.  there are many special cases;
   refer to the spec.
   Author:             christine@netscape.com
   Date:               9 july 1997
*/

var SECTION = "15.8.2.13";
var VERSION = "ECMA_1";
var TITLE   = "Math.pow(x, y)";
var BUGNUMBER="77141";

startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "Math.pow.length",
	      2,
	      Math.pow.length );

new TestCase( SECTION,
	      "Math.pow()",
	      Number.NaN,
	      Math.pow() );

new TestCase( SECTION,
	      "Math.pow(null, null)",
	      1,
	      Math.pow(null,null) );

new TestCase( SECTION, 
	      "Math.pow(void 0, void 0)",
	      Number.NaN,
              Math.pow(void 0, void 0));

new TestCase( SECTION, 
	      "Math.pow(true, false)",
	      1,
	      Math.pow(true, false) );

new TestCase( SECTION, 
	      "Math.pow(false,true)",
	      0,
	      Math.pow(false,true) );

new TestCase( SECTION, 
	      "Math.pow('2','32')",
	      4294967296,
              Math.pow('2','32') );

new TestCase( SECTION, 
	      "Math.pow(1,NaN)",
	      Number.NaN,
              Math.pow(1,Number.NaN) );

new TestCase( SECTION, 
	      "Math.pow(0,NaN)",	
	      Number.NaN,
              Math.pow(0,Number.NaN) );

new TestCase( SECTION, 
	      "Math.pow(NaN,0)",
	      1,
	      Math.pow(Number.NaN,0) );

new TestCase( SECTION,
	      "Math.pow(NaN,-0)",
              1,
	      Math.pow(Number.NaN,-0) );

new TestCase( SECTION, 
	      "Math.pow(NaN,1)",
	      Number.NaN,
              Math.pow(Number.NaN, 1) );

new TestCase( SECTION, 
	      "Math.pow(NaN,.5)",
              Number.NaN,
              Math.pow(Number.NaN, .5) );

new TestCase( SECTION, 
	      "Math.pow(1.00000001, Infinity)",
	      Number.POSITIVE_INFINITY,
	      Math.pow(1.00000001, Number.POSITIVE_INFINITY) );

new TestCase( SECTION, 
	      "Math.pow(1.00000001, -Infinity)", 
	      0,
	      Math.pow(1.00000001, Number.NEGATIVE_INFINITY) );

new TestCase( SECTION, 
	      "Math.pow(-1.00000001, Infinity)", 
	      Number.POSITIVE_INFINITY,
	      Math.pow(-1.00000001,Number.POSITIVE_INFINITY) );

new TestCase( SECTION, 
	      "Math.pow(-1.00000001, -Infinity)",
	      0,
	      Math.pow(-1.00000001,Number.NEGATIVE_INFINITY) );

new TestCase( SECTION, 
	      "Math.pow(1, Infinity)",
	      Number.NaN,
              Math.pow(1, Number.POSITIVE_INFINITY) );

new TestCase( SECTION, 
	      "Math.pow(1, -Infinity)",
	      Number.NaN,
              Math.pow(1, Number.NEGATIVE_INFINITY) );

new TestCase( SECTION, 
	      "Math.pow(-1, Infinity)",
	      Number.NaN,
              Math.pow(-1, Number.POSITIVE_INFINITY) );

new TestCase( SECTION, 
	      "Math.pow(-1, -Infinity)",
	      Number.NaN,
              Math.pow(-1, Number.NEGATIVE_INFINITY) );

new TestCase( SECTION, 
	      "Math.pow(.0000000009, Infinity)", 
	      0,
	      Math.pow(.0000000009, Number.POSITIVE_INFINITY) );

new TestCase( SECTION, 
	      "Math.pow(-.0000000009, Infinity)",
	      0,
	      Math.pow(-.0000000009, Number.POSITIVE_INFINITY) );

new TestCase( SECTION, 
	      "Math.pow(.0000000009, -Infinity)",
	      Number.POSITIVE_INFINITY,
	      Math.pow(-.0000000009, Number.NEGATIVE_INFINITY) );

new TestCase( SECTION, 
	      "Math.pow(Infinity, .00000000001)",
	      Number.POSITIVE_INFINITY,
	      Math.pow(Number.POSITIVE_INFINITY,.00000000001) );

new TestCase( SECTION, 
	      "Math.pow(Infinity, 1)",
	      Number.POSITIVE_INFINITY,
	      Math.pow(Number.POSITIVE_INFINITY, 1) );

new TestCase( SECTION, 
	      "Math.pow(Infinity, -.00000000001)",
	      0,
	      Math.pow(Number.POSITIVE_INFINITY, -.00000000001) );

new TestCase( SECTION, 
	      "Math.pow(Infinity, -1)",
	      0,
	      Math.pow(Number.POSITIVE_INFINITY, -1) );

new TestCase( SECTION, 
	      "Math.pow(-Infinity, 1)",
	      Number.NEGATIVE_INFINITY,
	      Math.pow(Number.NEGATIVE_INFINITY, 1) );

new TestCase( SECTION, 
	      "Math.pow(-Infinity, 333)",
	      Number.NEGATIVE_INFINITY,
	      Math.pow(Number.NEGATIVE_INFINITY, 333) );

new TestCase( SECTION, 
	      "Math.pow(Infinity, 2)",
	      Number.POSITIVE_INFINITY,
	      Math.pow(Number.POSITIVE_INFINITY, 2) );

new TestCase( SECTION, 
	      "Math.pow(-Infinity, 666)",
	      Number.POSITIVE_INFINITY,
	      Math.pow(Number.NEGATIVE_INFINITY, 666) );

new TestCase( SECTION, 
	      "Math.pow(-Infinity, 0.5)",
	      Number.POSITIVE_INFINITY,
	      Math.pow(Number.NEGATIVE_INFINITY, 0.5) );

new TestCase( SECTION, 
	      "Math.pow(-Infinity, Infinity)",
	      Number.POSITIVE_INFINITY,
	      Math.pow(Number.NEGATIVE_INFINITY, Number.POSITIVE_INFINITY) );

new TestCase( SECTION, 
	      "Math.pow(-Infinity, -1)",
	      -0,
	      Math.pow(Number.NEGATIVE_INFINITY, -1) );

new TestCase( SECTION, 
	      "Infinity/Math.pow(-Infinity, -1)",
	      -Infinity,
	      Infinity/Math.pow(Number.NEGATIVE_INFINITY, -1) );

new TestCase( SECTION, 
	      "Math.pow(-Infinity, -3)",
	      -0,
	      Math.pow(Number.NEGATIVE_INFINITY, -3) );

new TestCase( SECTION, 
	      "Math.pow(-Infinity, -2)",
	      0,
	      Math.pow(Number.NEGATIVE_INFINITY, -2) );

new TestCase( SECTION, 
	      "Math.pow(-Infinity, -0.5)",
	      0,
	      Math.pow(Number.NEGATIVE_INFINITY,-0.5) );

new TestCase( SECTION, 
	      "Math.pow(-Infinity, -Infinity)",
	      0,
	      Math.pow(Number.NEGATIVE_INFINITY, Number.NEGATIVE_INFINITY) );

new TestCase( SECTION, 
	      "Math.pow(0, 1)",
	      0,
	      Math.pow(0,1) );

new TestCase( SECTION, 
	      "Math.pow(0, 0)",
	      1,
	      Math.pow(0,0) );

new TestCase( SECTION, 
	      "Math.pow(1, 0)",
	      1,
	      Math.pow(1,0) );

new TestCase( SECTION,
	      "Math.pow(-1, 0)",
	      1,
	      Math.pow(-1,0) );

new TestCase( SECTION, 
	      "Math.pow(0, 0.5)",
              0,
	      Math.pow(0,0.5) );

new TestCase( SECTION, 
	      "Math.pow(0, 1000)",
	      0,
	      Math.pow(0,1000) );

new TestCase( SECTION,
	      "Math.pow(0, Infinity)",
	      0,
	      Math.pow(0, Number.POSITIVE_INFINITY) );

new TestCase( SECTION, 
	      "Math.pow(0, -1)",
	      Number.POSITIVE_INFINITY,
	      Math.pow(0, -1) );

new TestCase( SECTION, 
	      "Math.pow(0, -0.5)",
	      Number.POSITIVE_INFINITY,
	      Math.pow(0, -0.5) );

new TestCase( SECTION, 
	      "Math.pow(0, -1000)",
	      Number.POSITIVE_INFINITY,
	      Math.pow(0, -1000) );

new TestCase( SECTION, 
	      "Math.pow(0, -Infinity)",
	      Number.POSITIVE_INFINITY,
	      Math.pow(0, Number.NEGATIVE_INFINITY) );

new TestCase( SECTION, 
	      "Math.pow(-0, 1)",
	      -0,
	      Math.pow(-0, 1) );

new TestCase( SECTION, 
	      "Math.pow(-0, 3)",
	      -0,
	      Math.pow(-0,3) );

new TestCase( SECTION, 
	      "Infinity/Math.pow(-0, 1)",
	      -Infinity,
	      Infinity/Math.pow(-0, 1) );

new TestCase( SECTION, 
	      "Infinity/Math.pow(-0, 3)",
	      -Infinity,
	      Infinity/Math.pow(-0,3) );

new TestCase( SECTION, 
	      "Math.pow(-0, 2)",
	      0,
	      Math.pow(-0,2) );

new TestCase( SECTION, 
	      "Math.pow(-0, Infinity)",
	      0,
	      Math.pow(-0, Number.POSITIVE_INFINITY) );

new TestCase( SECTION, 
	      "Math.pow(-0, -1)",
              Number.NEGATIVE_INFINITY,
	      Math.pow(-0, -1) );

new TestCase( SECTION, 
	      "Math.pow(-0, -10001)",
	      Number.NEGATIVE_INFINITY,
	      Math.pow(-0, -10001) );

new TestCase( SECTION, 
	      "Math.pow(-0, -2)",
              Number.POSITIVE_INFINITY,
	      Math.pow(-0, -2) );

new TestCase( SECTION, 
	      "Math.pow(-0, 0.5)",
	      0,
	      Math.pow(-0, 0.5) );

new TestCase( SECTION, 
	      "Math.pow(-0, Infinity)",
	      0,
	      Math.pow(-0, Number.POSITIVE_INFINITY) );

new TestCase( SECTION, 
	      "Math.pow(-1, 0.5)",
	      Number.NaN,
              Math.pow(-1, 0.5) );

new TestCase( SECTION, 
	      "Math.pow(-1, NaN)",
	      Number.NaN,
              Math.pow(-1, Number.NaN) );

new TestCase( SECTION, 
	      "Math.pow(-1, -0.5)",
	      Number.NaN,
              Math.pow(-1, -0.5) );

test();
