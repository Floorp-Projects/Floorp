/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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
var TITLE   = "ToNumber";

writeHeaderToLog( SECTION + " "+ TITLE);

// special cases here

new TestCase( "Number()",                      0,              Number() );
new TestCase( "Number(eval('var x'))",         Number.NaN,     Number(eval("var x")) );
new TestCase( "Number(void 0)",                Number.NaN,     Number(void 0) );
new TestCase( "Number(null)",                  0,              Number(null) );
new TestCase( "Number(true)",                  1,              Number(true) );
new TestCase( "Number(false)",                 0,              Number(false) );
new TestCase( "Number(0)",                     0,              Number(0) );
new TestCase( "Number(-0)",                    -0,             Number(-0) );
new TestCase( "Number(1)",                     1,              Number(1) );
new TestCase( "Number(-1)",                    -1,             Number(-1) );
new TestCase( "Number(Number.MAX_VALUE)",      1.7976931348623157e308, Number(Number.MAX_VALUE) );
new TestCase( "Number(Number.MIN_VALUE)",      5e-324,         Number(Number.MIN_VALUE) );

new TestCase( "Number(Number.NaN)",                Number.NaN,                 Number(Number.NaN) );
new TestCase( "Number(Number.POSITIVE_INFINITY)",  Number.POSITIVE_INFINITY,   Number(Number.POSITIVE_INFINITY) );
new TestCase( "Number(Number.NEGATIVE_INFINITY)",  Number.NEGATIVE_INFINITY,   Number(Number.NEGATIVE_INFINITY) );

test();
