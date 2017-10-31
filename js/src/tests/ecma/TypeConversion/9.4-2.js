/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          9.4-1.js
   ECMA Section:       9.4 ToInteger
   Description:        1.  Call ToNumber on the input argument
   2.  If Result(1) is NaN, return +0
   3.  If Result(1) is +0, -0, Infinity, or -Infinity,
   return Result(1).
   4.  Compute sign(Result(1)) * floor(abs(Result(1))).
   5.  Return Result(4).

   To test ToInteger, this test uses new Date(value),
   15.9.3.7.  The Date constructor sets the [[Value]]
   property of the new object to TimeClip(value), which
   uses the rules:

   TimeClip(time)
   1. If time is not finite, return NaN
   2. If abs(Result(1)) > 8.64e15, return NaN
   3. Return an implementation dependent choice of either
   ToInteger(Result(2)) or ToInteger(Result(2)) + (+0)
   (Adding a positive 0 converts -0 to +0).

   This tests ToInteger for values -8.64e15 > value > 8.64e15,
   not including -0 and +0.

   For additional special cases (0, +0, Infinity, -Infinity,
   and NaN, see 9.4-2.js).  For value is String, see 9.4-3.js.

   Author:             christine@netscape.com
   Date:               10 july 1997

*/
var SECTION = "9.4-1";
var TITLE   = "ToInteger";

writeHeaderToLog( SECTION + " "+ TITLE);

// some special cases

new TestCase( "td = new Date(Number.NaN); td.valueOf()",  Number.NaN, eval("td = new Date(Number.NaN); td.valueOf()") );
new TestCase( "td = new Date(Infinity); td.valueOf()",    Number.NaN, eval("td = new Date(Number.POSITIVE_INFINITY); td.valueOf()") );
new TestCase( "td = new Date(-Infinity); td.valueOf()",   Number.NaN, eval("td = new Date(Number.NEGATIVE_INFINITY); td.valueOf()") );
new TestCase( "td = new Date(-0); td.valueOf()",          -0,         eval("td = new Date(-0); td.valueOf()" ) );
new TestCase( "td = new Date(0); td.valueOf()",           0,          eval("td = new Date(0); td.valueOf()") );

// value is not an integer

new TestCase( "td = new Date(3.14159); td.valueOf()",     3,          eval("td = new Date(3.14159); td.valueOf()") );
new TestCase( "td = new Date(Math.PI); td.valueOf()",     3,          eval("td = new Date(Math.PI); td.valueOf()") );
new TestCase( "td = new Date(-Math.PI);td.valueOf()",     -3,         eval("td = new Date(-Math.PI);td.valueOf()") );
new TestCase( "td = new Date(3.14159e2); td.valueOf()",   314,        eval("td = new Date(3.14159e2); td.valueOf()") );

new TestCase( "td = new Date(.692147e1); td.valueOf()",   6,          eval("td = new Date(.692147e1);td.valueOf()") );
new TestCase( "td = new Date(-.692147e1);td.valueOf()",   -6,         eval("td = new Date(-.692147e1);td.valueOf()") );

// value is not a number

new TestCase( "td = new Date(true); td.valueOf()",        1,          eval("td = new Date(true); td.valueOf()" ) );
new TestCase( "td = new Date(false); td.valueOf()",       0,          eval("td = new Date(false); td.valueOf()") );

new TestCase( "td = new Date(new Number(Math.PI)); td.valueOf()",  3, eval("td = new Date(new Number(Math.PI)); td.valueOf()") );
new TestCase( "td = new Date(new Number(Math.PI)); td.valueOf()",  3, eval("td = new Date(new Number(Math.PI)); td.valueOf()") );

// edge cases
new TestCase( "td = new Date(8.64e15); td.valueOf()",     8.64e15,    eval("td = new Date(8.64e15); td.valueOf()") );
new TestCase( "td = new Date(-8.64e15); td.valueOf()",    -8.64e15,   eval("td = new Date(-8.64e15); td.valueOf()") );
new TestCase( "td = new Date(8.64e-15); td.valueOf()",    0,          eval("td = new Date(8.64e-15); td.valueOf()") );
new TestCase( "td = new Date(-8.64e-15); td.valueOf()",   0,          eval("td = new Date(-8.64e-15); td.valueOf()") );

test();
