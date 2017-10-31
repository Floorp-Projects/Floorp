/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.8.2.18.js
   ECMA Section:       15.8.2.18 tan( x )
   Description:        return an approximation to the tan of the
   argument.  argument is expressed in radians
   special cases:
   - if x is NaN           result is NaN
   - if x is 0             result is 0
   - if x is -0            result is -0
   - if x is Infinity or -Infinity result is NaN
   Author:             christine@netscape.com
   Date:               7 july 1997
*/

var SECTION = "15.8.2.18";
var TITLE   = "Math.tan(x)";
var EXCLUDE = "true";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "Math.tan.length",
	      1,
	      Math.tan.length );

new TestCase( "Math.tan()",
	      Number.NaN,
	      Math.tan() );

new TestCase( "Math.tan(void 0)",
	      Number.NaN,
	      Math.tan(void 0));

new TestCase( "Math.tan(null)",
	      0,
	      Math.tan(null) );

new TestCase( "Math.tan(false)",
	      0,
	      Math.tan(false) );

new TestCase( "Math.tan(NaN)",
	      Number.NaN,
	      Math.tan(Number.NaN) );

new TestCase( "Math.tan(0)",
	      0,
	      Math.tan(0));

new TestCase( "Math.tan(-0)",
	      -0,
	      Math.tan(-0));

new TestCase( "Math.tan(Infinity)",
	      Number.NaN,
	      Math.tan(Number.POSITIVE_INFINITY));

new TestCase( "Math.tan(-Infinity)",
	      Number.NaN,
	      Math.tan(Number.NEGATIVE_INFINITY));

new TestCase( "Math.tan(Math.PI/4)",
	      1,
	      Math.tan(Math.PI/4));

new TestCase( "Math.tan(3*Math.PI/4)",
	      -1,
	      Math.tan(3*Math.PI/4));

new TestCase( "Math.tan(Math.PI)",
	      -0,
	      Math.tan(Math.PI));

new TestCase( "Math.tan(5*Math.PI/4)",
	      1,
	      Math.tan(5*Math.PI/4));

new TestCase( "Math.tan(7*Math.PI/4)",
	      -1,
	      Math.tan(7*Math.PI/4));

new TestCase( "Infinity/Math.tan(-0)",
	      -Infinity,
	      Infinity/Math.tan(-0) );

/*
  Arctan (x) ~ PI/2 - 1/x   for large x.  For x = 1.6x10^16, 1/x is about the last binary digit of double precision PI/2.
  That is to say, perturbing PI/2 by this much is about the smallest rounding error possible.

  This suggests that the answer Christine is getting and a real Infinity are "adjacent" results from the tangent function.  I
  suspect that tan (PI/2 + one ulp) is a negative result about the same size as tan (PI/2) and that this pair are the closest
  results to infinity that the algorithm can deliver.

  In any case, my call is that the answer we're seeing is "right".  I suggest the test pass on any result this size or larger.
  = C =
*/

new TestCase( "Math.tan(3*Math.PI/2) >= 5443000000000000",
	      true,
	      Math.tan(3*Math.PI/2) >= 5443000000000000 );

new TestCase( "Math.tan(Math.PI/2) >= 5443000000000000",
	      true,
	      Math.tan(Math.PI/2) >= 5443000000000000 );

test();
