/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          11.4.9.js
   ECMA Section:       11.4.9 Logical NOT Operator (!)
   Description:        if the ToBoolean( VALUE ) result is true, return
   true.  else return false.
   Author:             christine@netscape.com
   Date:               7 july 1997

   Static variables:
   none
*/
var SECTION = "11.4.9";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Logical NOT operator (!)";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,   "!(null)",                true,   !(null) );
new TestCase( SECTION,   "!(var x)",               true,   !(eval("var x")) );
new TestCase( SECTION,   "!(void 0)",              true,   !(void 0) );

new TestCase( SECTION,   "!(false)",               true,   !(false) );
new TestCase( SECTION,   "!(true)",                false,  !(true) );
new TestCase( SECTION,   "!()",                    true,   !(eval()) );
new TestCase( SECTION,   "!(0)",                   true,   !(0) );
new TestCase( SECTION,   "!(-0)",                  true,   !(-0) );
new TestCase( SECTION,   "!(NaN)",                 true,   !(Number.NaN) );
new TestCase( SECTION,   "!(Infinity)",            false,  !(Number.POSITIVE_INFINITY) );
new TestCase( SECTION,   "!(-Infinity)",           false,  !(Number.NEGATIVE_INFINITY) );
new TestCase( SECTION,   "!(Math.PI)",             false,  !(Math.PI) );
new TestCase( SECTION,   "!(1)",                   false,  !(1) );
new TestCase( SECTION,   "!(-1)",                  false,  !(-1) );
new TestCase( SECTION,   "!('')",                  true,   !("") );
new TestCase( SECTION,   "!('\t')",                false,  !("\t") );
new TestCase( SECTION,   "!('0')",                 false,  !("0") );
new TestCase( SECTION,   "!('string')",            false,  !("string") );
new TestCase( SECTION,   "!(new String(''))",      false,  !(new String("")) );
new TestCase( SECTION,   "!(new String('string'))",    false,  !(new String("string")) );
new TestCase( SECTION,   "!(new String())",        false,  !(new String()) );
new TestCase( SECTION,   "!(new Boolean(true))",   false,   !(new Boolean(true)) );
new TestCase( SECTION,   "!(new Boolean(false))",  false,   !(new Boolean(false)) );
new TestCase( SECTION,   "!(new Array())",         false,  !(new Array()) );
new TestCase( SECTION,   "!(new Array(1,2,3)",     false,  !(new Array(1,2,3)) );
new TestCase( SECTION,   "!(new Number())",        false,  !(new Number()) );
new TestCase( SECTION,   "!(new Number(0))",       false,  !(new Number(0)) );
new TestCase( SECTION,   "!(new Number(NaN))",     false,  !(new Number(Number.NaN)) );
new TestCase( SECTION,   "!(new Number(Infinity))", false, !(new Number(Number.POSITIVE_INFINITY)) );

test();

