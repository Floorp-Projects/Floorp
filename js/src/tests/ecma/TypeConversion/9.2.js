/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          9.2.js
   ECMA Section:       9.2  Type Conversion:  ToBoolean
   Description:        rules for converting an argument to a boolean.
   undefined           false
   Null                false
   Boolean             input argument( no conversion )
   Number              returns false for 0, -0, and NaN
   otherwise return true
   String              return false if the string is empty
   (length is 0) otherwise the result is
   true
   Object              all return true

   Author:             christine@netscape.com
   Date:               14 july 1997
*/
var SECTION = "9.2";
var TITLE   = "ToBoolean";

writeHeaderToLog( SECTION + " "+ TITLE);

// special cases here

new TestCase( "Boolean()",                     false,  Boolean() );
new TestCase( "Boolean(var x)",                false,  Boolean(eval("var x")) );
new TestCase( "Boolean(void 0)",               false,  Boolean(void 0) );
new TestCase( "Boolean(null)",                 false,  Boolean(null) );
new TestCase( "Boolean(false)",                false,  Boolean(false) );
new TestCase( "Boolean(true)",                 true,   Boolean(true) );
new TestCase( "Boolean(0)",                    false,  Boolean(0) );
new TestCase( "Boolean(-0)",                   false,  Boolean(-0) );
new TestCase( "Boolean(NaN)",                  false,  Boolean(Number.NaN) );
new TestCase( "Boolean('')",                   false,  Boolean("") );

// normal test cases here

new TestCase( "Boolean(Infinity)",             true,   Boolean(Number.POSITIVE_INFINITY) );
new TestCase( "Boolean(-Infinity)",            true,   Boolean(Number.NEGATIVE_INFINITY) );
new TestCase( "Boolean(Math.PI)",              true,   Boolean(Math.PI) );
new TestCase( "Boolean(1)",                    true,   Boolean(1) );
new TestCase( "Boolean(-1)",                   true,   Boolean(-1) );
new TestCase( "Boolean([tab])",                true,   Boolean("\t") );
new TestCase( "Boolean('0')",                  true,   Boolean("0") );
new TestCase( "Boolean('string')",             true,   Boolean("string") );

// ToBoolean (object) should always return true.
new TestCase( "Boolean(new String() )",        true,   Boolean(new String()) );
new TestCase( "Boolean(new String('') )",      true,   Boolean(new String("")) );

new TestCase( "Boolean(new Boolean(true))",    true,   Boolean(new Boolean(true)) );
new TestCase( "Boolean(new Boolean(false))",   true,   Boolean(new Boolean(false)) );
new TestCase( "Boolean(new Boolean() )",       true,   Boolean(new Boolean()) );

new TestCase( "Boolean(new Array())",          true,   Boolean(new Array()) );

new TestCase( "Boolean(new Number())",         true,   Boolean(new Number()) );
new TestCase( "Boolean(new Number(-0))",       true,   Boolean(new Number(-0)) );
new TestCase( "Boolean(new Number(0))",        true,   Boolean(new Number(0)) );
new TestCase( "Boolean(new Number(NaN))",      true,   Boolean(new Number(Number.NaN)) );

new TestCase( "Boolean(new Number(-1))",       true,   Boolean(new Number(-1)) );
new TestCase( "Boolean(new Number(Infinity))", true,   Boolean(new Number(Number.POSITIVE_INFINITY)) );
new TestCase( "Boolean(new Number(-Infinity))",true,   Boolean(new Number(Number.NEGATIVE_INFINITY)) );

new TestCase( "Boolean(new Object())",       true,       Boolean(new Object()) );
new TestCase( "Boolean(new Function())",     true,       Boolean(new Function()) );
new TestCase( "Boolean(new Date())",         true,       Boolean(new Date()) );
new TestCase( "Boolean(new Date(0))",         true,       Boolean(new Date(0)) );
new TestCase( "Boolean(Math)",         true,       Boolean(Math) );

// bug 375793
new TestCase( "NaN ? true : false",
              false,
              (NaN ? true : false) );
new TestCase( "1000 % 0 ? true : false",
              false,
              (1000 % 0 ? true : false) );
new TestCase( "(function(a,b){ return a % b ? true : false })(1000, 0)",
              false,
              ((function(a,b){ return a % b ? true : false })(1000, 0)) );

new TestCase( "(function(x) { return !(x) })(0/0)",
              true,
              ((function(x) { return !(x) })(0/0)) );
new TestCase( "!(0/0)",
              true,
              (!(0/0)) );
test();

