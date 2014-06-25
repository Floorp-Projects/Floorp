/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          9.3-1.js
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


   This tests ToNumber applied to the object type, except
   if object is string.  See 9.3-2 for
   ToNumber( String object).

   Author:             christine@netscape.com
   Date:               10 july 1997

*/
var SECTION = "9.3-1";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " ToNumber");

// object is Number
new TestCase( SECTION, "Number(new Number())", 0, Number(new Number()) );
new TestCase( SECTION, "typeof Number(new Number())", "number", typeof Number(new Number()) );

new TestCase( SECTION, "Number(new Number(Number.NaN))", Number.NaN, Number(new Number(Number.NaN)) );
new TestCase( SECTION, "typeof Number(new Number(Number.NaN))","number", typeof Number(new Number(Number.NaN)) );

new TestCase( SECTION, "Number(new Number(0))", 0, Number(new Number(0)) );
new TestCase( SECTION, "typeof Number(new Number(0))", "number", typeof Number(new Number(0)) );

new TestCase( SECTION, "Number(new Number(null))", 0, Number(new Number(null)) );
new TestCase( SECTION, "typeof Number(new Number(null))", "number", typeof Number(new Number(null)) );


// new TestCase( SECTION, "Number(new Number(void 0))", Number.NaN, Number(new Number(void 0)) );
new TestCase( SECTION, "Number(new Number(true))", 1, Number(new Number(true)) );
new TestCase( SECTION, "typeof Number(new Number(true))", "number", typeof Number(new Number(true)) );

new TestCase( SECTION, "Number(new Number(false))", 0, Number(new Number(false)) );
new TestCase( SECTION, "typeof Number(new Number(false))", "number", typeof Number(new Number(false)) );

// object is boolean
new TestCase( SECTION, "Number(new Boolean(true))", 1, Number(new Boolean(true)) );
new TestCase( SECTION, "typeof Number(new Boolean(true))", "number", typeof Number(new Boolean(true)) );

new TestCase( SECTION, "Number(new Boolean(false))", 0, Number(new Boolean(false)) );
new TestCase( SECTION, "typeof Number(new Boolean(false))", "number", typeof Number(new Boolean(false)) );

// object is array
new TestCase( SECTION, "Number(new Array(2,4,8,16,32))", Number.NaN, Number(new Array(2,4,8,16,32)) );
new TestCase( SECTION, "typeof Number(new Array(2,4,8,16,32))", "number", typeof Number(new Array(2,4,8,16,32)) );

test();
