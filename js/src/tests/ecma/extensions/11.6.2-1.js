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

// tests "MyValuelessObject", where the value is
// set in the object's prototype, not the object itself.


new TestCase(   SECTION,
                "var EXP_1 = new MyValuelessObject(true); var EXP_2 = new MyValuelessObject(false); EXP_1 - EXP_2",
                1,
                eval("var EXP_1 = new MyValuelessObject(true); var EXP_2 = new MyValuelessObject(false); EXP_1 - EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new MyValuelessObject(new Boolean(true)); var EXP_2 = new MyValuelessObject(new Boolean(false)); EXP_1 - EXP_2",
                Number.NaN,
                eval("var EXP_1 = new MyValuelessObject(new Boolean(true)); var EXP_2 = new MyValuelessObject(new Boolean(false)); EXP_1 - EXP_2") );

// tests "MyValuelessObject", where the value is
// set in the object's prototype, not the object itself.

new TestCase(   SECTION,
                "var EXP_1 = new MyValuelessObject(100); var EXP_2 = new MyValuelessObject(1); EXP_1 - EXP_2",
                99,
                eval("var EXP_1 = new MyValuelessObject(100); var EXP_2 = new MyValuelessObject(1); EXP_1 - EXP_2") );
/*
  new TestCase(   SECTION,
  "var EXP_1 = new MyValuelessObject(new Number(100)); var EXP_2 = new MyValuelessObject(new Number(1)); EXP_1 - EXP_2",
  Number.NaN,
  eval("var EXP_1 = new MyValuelessObject(new Number(100)); var EXP_2 = new MyValuelessObject(new Number(1)); EXP_1 - EXP_2") );
*/
// same thing with string!

test();

function MyProtoValuelessObject() {
  this.valueOf = new Function ( "" );
  this.__proto__ = null;
}
function MyProtolessObject( value ) {
  this.valueOf = new Function( "return this.value" );
  this.__proto__ = null;
  this.value = value;
}
function MyValuelessObject(value) {
  this.__proto__ = new MyPrototypeObject(value);
}
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
