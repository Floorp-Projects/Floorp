/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.5.4.4-1.js
   ECMA Section:       15.5.4.4 String.prototype.charAt(pos)
   Description:        Returns a string containing the character at position
   pos in the string.  If there is no character at that
   string, the result is the empty string.  The result is
   a string value, not a String object.

   When the charAt method is called with one argument,
   pos, the following steps are taken:
   1. Call ToString, with this value as its argument
   2. Call ToInteger pos
   3. Compute the number of characters  in Result(1)
   4. If Result(2) is less than 0 is or not less than
   Result(3), return the empty string
   5. Return a string of length 1 containing one character
   from result (1), the character at position Result(2).

   Note that the charAt function is intentionally generic;
   it does not require that its this value be a String
   object.  Therefore it can be transferred to other kinds
   of objects for use as a method.

   Author:             christine@netscape.com
   Date:               2 october 1997
*/
var SECTION = "15.5.4.4-2";
var TITLE   = "String.prototype.charAt";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(0)", "t",    eval("x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(0)") );
new TestCase( "x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(1)", "r",    eval("x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(1)") );
new TestCase( "x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(2)", "u",    eval("x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(2)") );
new TestCase( "x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(3)", "e",    eval("x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(3)") );
new TestCase( "x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(4)", "",     eval("x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(4)") );
new TestCase( "x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(-1)", "",    eval("x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(-1)") );

new TestCase( "x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(true)", "r",    eval("x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(true)") );
new TestCase( "x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(false)", "t",    eval("x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(false)") );

new TestCase( "x = new String(); x.charAt(0)",    "",     eval("x=new String();x.charAt(0)") );
new TestCase( "x = new String(); x.charAt(1)",    "",     eval("x=new String();x.charAt(1)") );
new TestCase( "x = new String(); x.charAt(-1)",   "",     eval("x=new String();x.charAt(-1)") );

new TestCase( "x = new String(); x.charAt(NaN)",  "",     eval("x=new String();x.charAt(Number.NaN)") );
new TestCase( "x = new String(); x.charAt(Number.POSITIVE_INFINITY)",   "",     eval("x=new String();x.charAt(Number.POSITIVE_INFINITY)") );
new TestCase( "x = new String(); x.charAt(Number.NEGATIVE_INFINITY)",   "",     eval("x=new String();x.charAt(Number.NEGATIVE_INFINITY)") );

new TestCase( "var MYOB = new MyObject(1234567890); MYOB.charAt(0)",  "1",        eval("var MYOB = new MyObject(1234567890); MYOB.charAt(0)") );
new TestCase( "var MYOB = new MyObject(1234567890); MYOB.charAt(1)",  "2",        eval("var MYOB = new MyObject(1234567890); MYOB.charAt(1)") );
new TestCase( "var MYOB = new MyObject(1234567890); MYOB.charAt(2)",  "3",        eval("var MYOB = new MyObject(1234567890); MYOB.charAt(2)") );
new TestCase( "var MYOB = new MyObject(1234567890); MYOB.charAt(3)",  "4",        eval("var MYOB = new MyObject(1234567890); MYOB.charAt(3)") );
new TestCase( "var MYOB = new MyObject(1234567890); MYOB.charAt(4)",  "5",        eval("var MYOB = new MyObject(1234567890); MYOB.charAt(4)") );
new TestCase( "var MYOB = new MyObject(1234567890); MYOB.charAt(5)",  "6",        eval("var MYOB = new MyObject(1234567890); MYOB.charAt(5)") );
new TestCase( "var MYOB = new MyObject(1234567890); MYOB.charAt(6)",  "7",        eval("var MYOB = new MyObject(1234567890); MYOB.charAt(6)") );
new TestCase( "var MYOB = new MyObject(1234567890); MYOB.charAt(7)",  "8",        eval("var MYOB = new MyObject(1234567890); MYOB.charAt(7)") );
new TestCase( "var MYOB = new MyObject(1234567890); MYOB.charAt(8)",  "9",        eval("var MYOB = new MyObject(1234567890); MYOB.charAt(8)") );
new TestCase( "var MYOB = new MyObject(1234567890); MYOB.charAt(9)",  "0",        eval("var MYOB = new MyObject(1234567890); MYOB.charAt(9)") );
new TestCase( "var MYOB = new MyObject(1234567890); MYOB.charAt(10)",  "",       eval("var MYOB = new MyObject(1234567890); MYOB.charAt(10)") );

new TestCase( "var MYOB = new MyObject(1234567890); MYOB.charAt(Math.PI)",  "4",        eval("var MYOB = new MyObject(1234567890); MYOB.charAt(Math.PI)") );

// MyOtherObject.toString will return "[object Object]

new TestCase( "var MYOB = new MyOtherObject(1234567890); MYOB.charAt(0)",  "[",        eval("var MYOB = new MyOtherObject(1234567890); MYOB.charAt(0)") );
new TestCase( "var MYOB = new MyOtherObject(1234567890); MYOB.charAt(1)",  "o",        eval("var MYOB = new MyOtherObject(1234567890); MYOB.charAt(1)") );
new TestCase( "var MYOB = new MyOtherObject(1234567890); MYOB.charAt(2)",  "b",        eval("var MYOB = new MyOtherObject(1234567890); MYOB.charAt(2)") );
new TestCase( "var MYOB = new MyOtherObject(1234567890); MYOB.charAt(3)",  "j",        eval("var MYOB = new MyOtherObject(1234567890); MYOB.charAt(3)") );
new TestCase( "var MYOB = new MyOtherObject(1234567890); MYOB.charAt(4)",  "e",        eval("var MYOB = new MyOtherObject(1234567890); MYOB.charAt(4)") );
new TestCase( "var MYOB = new MyOtherObject(1234567890); MYOB.charAt(5)",  "c",        eval("var MYOB = new MyOtherObject(1234567890); MYOB.charAt(5)") );
new TestCase( "var MYOB = new MyOtherObject(1234567890); MYOB.charAt(6)",  "t",        eval("var MYOB = new MyOtherObject(1234567890); MYOB.charAt(6)") );
new TestCase( "var MYOB = new MyOtherObject(1234567890); MYOB.charAt(7)",  " ",        eval("var MYOB = new MyOtherObject(1234567890); MYOB.charAt(7)") );
new TestCase( "var MYOB = new MyOtherObject(1234567890); MYOB.charAt(8)",  "O",        eval("var MYOB = new MyOtherObject(1234567890); MYOB.charAt(8)") );
new TestCase( "var MYOB = new MyOtherObject(1234567890); MYOB.charAt(9)",  "b",        eval("var MYOB = new MyOtherObject(1234567890); MYOB.charAt(9)") );
new TestCase( "var MYOB = new MyOtherObject(1234567890); MYOB.charAt(10)",  "j",        eval("var MYOB = new MyOtherObject(1234567890); MYOB.charAt(10)") );
new TestCase( "var MYOB = new MyOtherObject(1234567890); MYOB.charAt(11)",  "e",        eval("var MYOB = new MyOtherObject(1234567890); MYOB.charAt(11)") );
new TestCase( "var MYOB = new MyOtherObject(1234567890); MYOB.charAt(12)",  "c",        eval("var MYOB = new MyOtherObject(1234567890); MYOB.charAt(12)") );
new TestCase( "var MYOB = new MyOtherObject(1234567890); MYOB.charAt(13)",  "t",        eval("var MYOB = new MyOtherObject(1234567890); MYOB.charAt(13)") );
new TestCase( "var MYOB = new MyOtherObject(1234567890); MYOB.charAt(14)",  "]",        eval("var MYOB = new MyOtherObject(1234567890); MYOB.charAt(14)") );

test();

function MyObject( value ) {
  this.value      = value;
  this.valueOf    = new Function( "return this.value;" );
  this.toString   = new Function( "return this.value +''" );
  this.charAt     = String.prototype.charAt;
}
function MyOtherObject(value) {
  this.value      = value;
  this.valueOf    = new Function( "return this.value;" );
  this.charAt     = String.prototype.charAt;
}
