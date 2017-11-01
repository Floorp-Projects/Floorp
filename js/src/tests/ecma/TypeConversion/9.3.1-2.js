/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          9.3.1-2.js
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

   This tests special cases of ToNumber(string) that are
   not covered in 9.3.1-1.js.

   Author:             christine@netscape.com
   Date:               10 july 1997

*/
var SECTION = "9.3.1-2";
var TITLE   = "ToNumber applied to the String type";

writeHeaderToLog( SECTION + " "+ TITLE);

// A StringNumericLiteral may not use octal notation

new TestCase( "Number(00)",        0,         Number("00"));
new TestCase( "Number(01)",        1,         Number("01"));
new TestCase( "Number(02)",        2,         Number("02"));
new TestCase( "Number(03)",        3,         Number("03"));
new TestCase( "Number(04)",        4,         Number("04"));
new TestCase( "Number(05)",        5,         Number("05"));
new TestCase( "Number(06)",        6,         Number("06"));
new TestCase( "Number(07)",        7,         Number("07"));
new TestCase( "Number(010)",       10,        Number("010"));
new TestCase( "Number(011)",       11,        Number("011"));

// A StringNumericLIteral may have any number of leading 0 digits

new TestCase( "Number(001)",        1,         Number("001"));
new TestCase( "Number(0001)",       1,         Number("0001"));

test();

