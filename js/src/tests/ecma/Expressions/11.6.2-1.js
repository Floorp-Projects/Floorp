/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          11.6.2-1.js
   ECMA Section:       11.6.2 The Subtraction operator ( - )
   Description:

   The production AdditiveExpression : AdditiveExpression -
   MultiplicativeExpression is evaluated as follows:

   1.  Evaluate AdditiveExpression.
   2.  Call GetValue(Result(1)).
   3.  Evaluate MultiplicativeExpression.
   4.  Call GetValue(Result(3)).
   5.  Call ToNumber(Result(2)).
   6.  Call ToNumber(Result(4)).
   7.  Apply the subtraction operation to Result(5) and Result(6). See the
   discussion below (11.6.3).
   8.  Return Result(7).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.6.2-1";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " The subtraction operator ( - )");

// tests for boolean primitive, boolean object, Object object, a "MyObject" whose value is
// a boolean primitive and a boolean object.

new TestCase(   SECTION,
                "var EXP_1 = true; var EXP_2 = false; EXP_1 - EXP_2",
                1,
                eval("var EXP_1 = true; var EXP_2 = false; EXP_1 - EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new Boolean(true); var EXP_2 = new Boolean(false); EXP_1 - EXP_2",
                1,
                eval("var EXP_1 = new Boolean(true); var EXP_2 = new Boolean(false); EXP_1 - EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new Object(true); var EXP_2 = new Object(false); EXP_1 - EXP_2",
                1,
                eval("var EXP_1 = new Object(true); var EXP_2 = new Object(false); EXP_1 - EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new Object(new Boolean(true)); var EXP_2 = new Object(new Boolean(false)); EXP_1 - EXP_2",
                1,
                eval("var EXP_1 = new Object(new Boolean(true)); var EXP_2 = new Object(new Boolean(false)); EXP_1 - EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new MyObject(true); var EXP_2 = new MyObject(false); EXP_1 - EXP_2",
                1,
                eval("var EXP_1 = new MyObject(true); var EXP_2 = new MyObject(false); EXP_1 - EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new MyObject(new Boolean(true)); var EXP_2 = new MyObject(new Boolean(false)); EXP_1 - EXP_2",
                Number.NaN,
                eval("var EXP_1 = new MyObject(new Boolean(true)); var EXP_2 = new MyObject(new Boolean(false)); EXP_1 - EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new MyOtherObject(new Boolean(true)); var EXP_2 = new MyOtherObject(new Boolean(false)); EXP_1 - EXP_2",
                Number.NaN,
                eval("var EXP_1 = new MyOtherObject(new Boolean(true)); var EXP_2 = new MyOtherObject(new Boolean(false)); EXP_1 - EXP_2") );

// tests for number primitive, number object, Object object, a "MyObject" whose value is
// a number primitive and a number object.

new TestCase(   SECTION,
                "var EXP_1 = 100; var EXP_2 = 1; EXP_1 - EXP_2",
                99,
                eval("var EXP_1 = 100; var EXP_2 = 1; EXP_1 - EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new Number(100); var EXP_2 = new Number(1); EXP_1 - EXP_2",
                99,
                eval("var EXP_1 = new Number(100); var EXP_2 = new Number(1); EXP_1 - EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new Object(100); var EXP_2 = new Object(1); EXP_1 - EXP_2",
                99,
                eval("var EXP_1 = new Object(100); var EXP_2 = new Object(1); EXP_1 - EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new Object(new Number(100)); var EXP_2 = new Object(new Number(1)); EXP_1 - EXP_2",
                99,
                eval("var EXP_1 = new Object(new Number(100)); var EXP_2 = new Object(new Number(1)); EXP_1 - EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new MyObject(100); var EXP_2 = new MyObject(1); EXP_1 - EXP_2",
                99,
                eval("var EXP_1 = new MyObject(100); var EXP_2 = new MyObject(1); EXP_1 - EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new MyObject(new Number(100)); var EXP_2 = new MyObject(new Number(1)); EXP_1 - EXP_2",
                Number.NaN,
                eval("var EXP_1 = new MyObject(new Number(100)); var EXP_2 = new MyObject(new Number(1)); EXP_1 - EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new MyOtherObject(new Number(100)); var EXP_2 = new MyOtherObject(new Number(1)); EXP_1 - EXP_2",
                99,
                eval("var EXP_1 = new MyOtherObject(new Number(100)); var EXP_2 = new MyOtherObject(new Number(1)); EXP_1 - EXP_2") );

// same thing with string!
new TestCase(   SECTION,
                "var EXP_1 = new MyOtherObject(new String('0xff')); var EXP_2 = new MyOtherObject(new String('1'); EXP_1 - EXP_2",
                254,
                eval("var EXP_1 = new MyOtherObject(new String('0xff')); var EXP_2 = new MyOtherObject(new String('1')); EXP_1 - EXP_2") );

test();

function MyPrototypeObject(value) {
  this.valueOf = new Function( "return this.value;" );
  this.toString = new Function( "return (this.value + '');" );
  this.value = value;
}
function MyObject( value ) {
  this.valueOf = new Function( "return this.value" );
  this.value = value;
}
function MyOtherObject( value ) {
  this.valueOf = new Function( "return this.value" );
  this.toString = new Function ( "return this.value + ''" );
  this.value = value;
}
