/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          11.6.1-3.js
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

   This test does only covers cases where the Additive or Mulplicative expression
   is a Date.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.6.1-3";

writeHeaderToLog( SECTION + " The Addition operator ( + )");

// tests for a boolean primitive and a boolean object, and
// "MyValuelessObject", where the value is set in the object's
// prototype, not the object itself.

var DATE1 = new Date();

var MYOB1 = new MyObject( DATE1 );
var MYOB2 = new MyValuelessObject( DATE1 );
var MYOB3 = new MyProtolessObject( DATE1 );
var MYOB4 = new MyProtoValuelessObject( DATE1 );

new TestCase(   "MYOB2 = new MyValuelessObject(DATE1); MYOB3 + 'string'",
                DATE1.toString() + "string",
                MYOB2 + 'string' );

new TestCase(   "MYOB2 = new MyValuelessObject(DATE1); MYOB3 + new String('string')",
                DATE1.toString() + "string",
                MYOB2 + new String('string') );
/*
  new TestCase(
  "MYOB3 = new MyProtolessObject(DATE1); MYOB3 + new Boolean(true)",
  DATE1.toString() + "true",
  MYOB3 + new Boolean(true) );
*/

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
