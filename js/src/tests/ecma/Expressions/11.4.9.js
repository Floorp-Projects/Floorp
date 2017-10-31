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
var TITLE   = "Logical NOT operator (!)";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "!(null)",                true,   !(null) );
new TestCase( "!(var x)",               true,   !(eval("var x")) );
new TestCase( "!(void 0)",              true,   !(void 0) );

new TestCase( "!(false)",               true,   !(false) );
new TestCase( "!(true)",                false,  !(true) );
new TestCase( "!()",                    true,   !(eval()) );
new TestCase( "!(0)",                   true,   !(0) );
new TestCase( "!(-0)",                  true,   !(-0) );
new TestCase( "!(NaN)",                 true,   !(Number.NaN) );
new TestCase( "!(Infinity)",            false,  !(Number.POSITIVE_INFINITY) );
new TestCase( "!(-Infinity)",           false,  !(Number.NEGATIVE_INFINITY) );
new TestCase( "!(Math.PI)",             false,  !(Math.PI) );
new TestCase( "!(1)",                   false,  !(1) );
new TestCase( "!(-1)",                  false,  !(-1) );
new TestCase( "!('')",                  true,   !("") );
new TestCase( "!('\t')",                false,  !("\t") );
new TestCase( "!('0')",                 false,  !("0") );
new TestCase( "!('string')",            false,  !("string") );
new TestCase( "!(new String(''))",      false,  !(new String("")) );
new TestCase( "!(new String('string'))",    false,  !(new String("string")) );
new TestCase( "!(new String())",        false,  !(new String()) );
new TestCase( "!(new Boolean(true))",   false,   !(new Boolean(true)) );
new TestCase( "!(new Boolean(false))",  false,   !(new Boolean(false)) );
new TestCase( "!(new Array())",         false,  !(new Array()) );
new TestCase( "!(new Array(1,2,3)",     false,  !(new Array(1,2,3)) );
new TestCase( "!(new Number())",        false,  !(new Number()) );
new TestCase( "!(new Number(0))",       false,  !(new Number(0)) );
new TestCase( "!(new Number(NaN))",     false,  !(new Number(Number.NaN)) );
new TestCase( "!(new Number(Infinity))", false, !(new Number(Number.POSITIVE_INFINITY)) );

test();

