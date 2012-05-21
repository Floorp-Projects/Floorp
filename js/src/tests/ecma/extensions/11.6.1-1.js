/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          11.6.1-1.js
   ECMA Section:       11.6.1 The addition operator ( + )
   Description:

   The addition operator either performs string concatenation or numeric
   addition.

   The production AdditiveExpression : AdditiveExpression + MultiplicativeExpression
   is evaluated as follows:

   1.  Evaluate AdditiveExpression.
   2.  Call GetValue(Result(1)).
   3.  Evaluate MultiplicativeExpression.
   4.  Call GetValue(Result(3)).
   5.  Call ToPrimitive(Result(2)).
   6.  Call ToPrimitive(Result(4)).
   7.  If Type(Result(5)) is String or Type(Result(6)) is String, go to step 12.
   (Note that this step differs from step 3 in the algorithm for comparison
   for the relational operators in using or instead of and.)
   8.  Call ToNumber(Result(5)).
   9.  Call ToNumber(Result(6)).
   10. Apply the addition operation to Result(8) and Result(9). See the discussion below (11.6.3).
   11. Return Result(10).
   12. Call ToString(Result(5)).
   13. Call ToString(Result(6)).
   14. Concatenate Result(12) followed by Result(13).
   15. Return Result(14).

   Note that no hint is provided in the calls to ToPrimitive in steps 5 and 6.
   All native ECMAScript objects except Date objects handle the absence of a
   hint as if the hint Number were given; Date objects handle the absence of a
   hint as if the hint String were given. Host objects may handle the absence
   of a hint in some other manner.

   This test does not cover cases where the Additive or Mulplicative expression
   ToPrimitive is string.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.6.1-1";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " The Addition operator ( + )");

// tests for "MyValuelessObject", where the value is
// set in the object's prototype, not the object itself.

new TestCase(   SECTION,
		"var EXP_1 = new MyValuelessObject(true); var EXP_2 = new MyValuelessObject(false); EXP_1 + EXP_2",
		1,
		eval("var EXP_1 = new MyValuelessObject(true); var EXP_2 = new MyValuelessObject(false); EXP_1 + EXP_2") );

new TestCase(   SECTION,
		"var EXP_1 = new MyValuelessObject(new Boolean(true)); var EXP_2 = new MyValuelessObject(new Boolean(false)); EXP_1 + EXP_2",
		"truefalse",
		eval("var EXP_1 = new MyValuelessObject(new Boolean(true)); var EXP_2 = new MyValuelessObject(new Boolean(false)); EXP_1 + EXP_2") );

// tests for "MyValuelessObject", where the value is
// set in the object's prototype, not the object itself.


new TestCase(   SECTION,
		"var EXP_1 = new MyValuelessObject(100); var EXP_2 = new MyValuelessObject(-1); EXP_1 + EXP_2",
		99,
		eval("var EXP_1 = new MyValuelessObject(100); var EXP_2 = new MyValuelessObject(-1); EXP_1 + EXP_2") );

new TestCase(   SECTION,
		"var EXP_1 = new MyValuelessObject(new Number(100)); var EXP_2 = new MyValuelessObject(new Number(-1)); EXP_1 + EXP_2",
		"100-1",
		eval("var EXP_1 = new MyValuelessObject(new Number(100)); var EXP_2 = new MyValuelessObject(new Number(-1)); EXP_1 + EXP_2") );

new TestCase(   SECTION,
		"var EXP_1 = new MyValuelessObject( new MyValuelessObject( new Boolean(true) ) ); EXP_1 + EXP_1",
		"truetrue",
		eval("var EXP_1 = new MyValuelessObject( new MyValuelessObject( new Boolean(true) ) ); EXP_1 + EXP_1") );

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
