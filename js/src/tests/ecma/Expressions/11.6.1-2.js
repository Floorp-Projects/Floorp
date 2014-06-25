/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          11.6.1-2.js
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
   ToPrimitive is a string.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.6.1-2";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " The Addition operator ( + )");

// tests for boolean primitive, boolean object, Object object, a "MyObject" whose value is
// a boolean primitive and a boolean object.

new TestCase(   SECTION,
                "var EXP_1 = 'string'; var EXP_2 = false; EXP_1 + EXP_2",
                "stringfalse",
                eval("var EXP_1 = 'string'; var EXP_2 = false; EXP_1 + EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = true; var EXP_2 = 'string'; EXP_1 + EXP_2",
                "truestring",
                eval("var EXP_1 = true; var EXP_2 = 'string'; EXP_1 + EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new Boolean(true); var EXP_2 = new String('string'); EXP_1 + EXP_2",
                "truestring",
                eval("var EXP_1 = new Boolean(true); var EXP_2 = new String('string'); EXP_1 + EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new Object(true); var EXP_2 = new Object('string'); EXP_1 + EXP_2",
                "truestring",
                eval("var EXP_1 = new Object(true); var EXP_2 = new Object('string'); EXP_1 + EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new Object(new String('string')); var EXP_2 = new Object(new Boolean(false)); EXP_1 + EXP_2",
                "stringfalse",
                eval("var EXP_1 = new Object(new String('string')); var EXP_2 = new Object(new Boolean(false)); EXP_1 + EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new MyObject(true); var EXP_2 = new MyObject('string'); EXP_1 + EXP_2",
                "truestring",
                eval("var EXP_1 = new MyObject(true); var EXP_2 = new MyObject('string'); EXP_1 + EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new MyObject(new String('string')); var EXP_2 = new MyObject(new Boolean(false)); EXP_1 + EXP_2",
                "[object Object][object Object]",
                eval("var EXP_1 = new MyObject(new String('string')); var EXP_2 = new MyObject(new Boolean(false)); EXP_1 + EXP_2") );

// tests for number primitive, number object, Object object, a "MyObject" whose value is
// a number primitive and a number object.

new TestCase(   SECTION,
                "var EXP_1 = 100; var EXP_2 = 'string'; EXP_1 + EXP_2",
                "100string",
                eval("var EXP_1 = 100; var EXP_2 = 'string'; EXP_1 + EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new String('string'); var EXP_2 = new Number(-1); EXP_1 + EXP_2",
                "string-1",
                eval("var EXP_1 = new String('string'); var EXP_2 = new Number(-1); EXP_1 + EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new Object(100); var EXP_2 = new Object('string'); EXP_1 + EXP_2",
                "100string",
                eval("var EXP_1 = new Object(100); var EXP_2 = new Object('string'); EXP_1 + EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new Object(new String('string')); var EXP_2 = new Object(new Number(-1)); EXP_1 + EXP_2",
                "string-1",
                eval("var EXP_1 = new Object(new String('string')); var EXP_2 = new Object(new Number(-1)); EXP_1 + EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new MyObject(100); var EXP_2 = new MyObject('string'); EXP_1 + EXP_2",
                "100string",
                eval("var EXP_1 = new MyObject(100); var EXP_2 = new MyObject('string'); EXP_1 + EXP_2") );

new TestCase(   SECTION,
                "var EXP_1 = new MyObject(new String('string')); var EXP_2 = new MyObject(new Number(-1)); EXP_1 + EXP_2",
                "[object Object][object Object]",
                eval("var EXP_1 = new MyObject(new String('string')); var EXP_2 = new MyObject(new Number(-1)); EXP_1 + EXP_2") );

test();

function MyObject( value ) {
  this.valueOf = new Function( "return this.value" );
  this.value = value;
}
