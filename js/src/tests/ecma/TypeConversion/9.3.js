/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          9.3.js
   ECMA Section:       9.3  Type Conversion:  ToNumber
   Description:        rules for converting an argument to a number.
   see 9.3.1 for cases for converting strings to numbers.
   special cases:
   undefined           NaN
   Null                NaN
   Boolean             1 if true; +0 if false
   Number              the argument ( no conversion )
   String              see test 9.3.1
   Object              see test 9.3-1

   For ToNumber applied to the String type, see test 9.3.1.
   For ToNumber applied to the object type, see test 9.3-1.

   Author:             christine@netscape.com
   Date:               10 july 1997

*/
var SECTION = "9.3";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "ToNumber";

writeHeaderToLog( SECTION + " "+ TITLE);

// special cases here

new TestCase( SECTION,   "Number()",                      0,              Number() );
new TestCase( SECTION,   "Number(eval('var x'))",         Number.NaN,     Number(eval("var x")) );
new TestCase( SECTION,   "Number(void 0)",                Number.NaN,     Number(void 0) );
new TestCase( SECTION,   "Number(null)",                  0,              Number(null) );
new TestCase( SECTION,   "Number(true)",                  1,              Number(true) );
new TestCase( SECTION,   "Number(false)",                 0,              Number(false) );
new TestCase( SECTION,   "Number(0)",                     0,              Number(0) );
new TestCase( SECTION,   "Number(-0)",                    -0,             Number(-0) );
new TestCase( SECTION,   "Number(1)",                     1,              Number(1) );
new TestCase( SECTION,   "Number(-1)",                    -1,             Number(-1) );
new TestCase( SECTION,   "Number(Number.MAX_VALUE)",      1.7976931348623157e308, Number(Number.MAX_VALUE) );
new TestCase( SECTION,   "Number(Number.MIN_VALUE)",      5e-324,         Number(Number.MIN_VALUE) );

new TestCase( SECTION,   "Number(Number.NaN)",                Number.NaN,                 Number(Number.NaN) );
new TestCase( SECTION,   "Number(Number.POSITIVE_INFINITY)",  Number.POSITIVE_INFINITY,   Number(Number.POSITIVE_INFINITY) );
new TestCase( SECTION,   "Number(Number.NEGATIVE_INFINITY)",  Number.NEGATIVE_INFINITY,   Number(Number.NEGATIVE_INFINITY) );

test();
