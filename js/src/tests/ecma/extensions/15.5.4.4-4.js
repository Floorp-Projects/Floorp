/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.5.4.4-4.js
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

   This tests assiging charAt to primitive types..

   Author:             christine@netscape.com
   Date:               2 october 1997
*/
var SECTION = "15.5.4.4-4";
var VERSION = "ECMA_2";
startTest();
var TITLE   = "String.prototype.charAt";

writeHeaderToLog( SECTION + " "+ TITLE);

/*
  new TestCase( SECTION,     "x = null; x.__proto.charAt = String.prototype.charAt; x.charAt(0)",            "n",     eval("x=null; x.__proto__.charAt = String.prototype.charAt; x.charAt(0)") );
  new TestCase( SECTION,     "x = null; x.__proto.charAt = String.prototype.charAt; x.charAt(1)",            "u",     eval("x=null; x.__proto__.charAt = String.prototype.charAt; x.charAt(1)") );
  new TestCase( SECTION,     "x = null; x.__proto.charAt = String.prototype.charAt; x.charAt(2)",            "l",     eval("x=null; x.__proto__.charAt = String.prototype.charAt; x.charAt(2)") );
  new TestCase( SECTION,     "x = null; x.__proto.charAt = String.prototype.charAt; x.charAt(3)",            "l",     eval("x=null; x.__proto__.charAt = String.prototype.charAt; x.charAt(3)") );

  new TestCase( SECTION,     "x = undefined; x.__proto.charAt = String.prototype.charAt; x.charAt(0)",            "u",     eval("x=undefined; x.__proto__.charAt = String.prototype.charAt; x.charAt(0)") );
  new TestCase( SECTION,     "x = undefined; x.__proto.charAt = String.prototype.charAt; x.charAt(1)",            "n",     eval("x=undefined; x.__proto__.charAt = String.prototype.charAt; x.charAt(1)") );
  new TestCase( SECTION,     "x = undefined; x.__proto.charAt = String.prototype.charAt; x.charAt(2)",            "d",     eval("x=undefined; x.__proto__.charAt = String.prototype.charAt; x.charAt(2)") );
  new TestCase( SECTION,     "x = undefined; x.__proto.charAt = String.prototype.charAt; x.charAt(3)",            "e",     eval("x=undefined; x.__proto__.charAt = String.prototype.charAt; x.charAt(3)") );
*/
new TestCase( SECTION,     "x = false; x.__proto.charAt = String.prototype.charAt; x.charAt(0)",            "f",     eval("x=false; x.__proto__.charAt = String.prototype.charAt; x.charAt(0)") );
new TestCase( SECTION,     "x = false; x.__proto.charAt = String.prototype.charAt; x.charAt(1)",            "a",     eval("x=false; x.__proto__.charAt = String.prototype.charAt; x.charAt(1)") );
new TestCase( SECTION,     "x = false; x.__proto.charAt = String.prototype.charAt; x.charAt(2)",            "l",     eval("x=false; x.__proto__.charAt = String.prototype.charAt; x.charAt(2)") );
new TestCase( SECTION,     "x = false; x.__proto.charAt = String.prototype.charAt; x.charAt(3)",            "s",     eval("x=false; x.__proto__.charAt = String.prototype.charAt; x.charAt(3)") );
new TestCase( SECTION,     "x = false; x.__proto.charAt = String.prototype.charAt; x.charAt(4)",            "e",     eval("x=false; x.__proto__.charAt = String.prototype.charAt; x.charAt(4)") );

new TestCase( SECTION,     "x = true; x.__proto.charAt = String.prototype.charAt; x.charAt(0)",            "t",     eval("x=true; x.__proto__.charAt = String.prototype.charAt; x.charAt(0)") );
new TestCase( SECTION,     "x = true; x.__proto.charAt = String.prototype.charAt; x.charAt(1)",            "r",     eval("x=true; x.__proto__.charAt = String.prototype.charAt; x.charAt(1)") );
new TestCase( SECTION,     "x = true; x.__proto.charAt = String.prototype.charAt; x.charAt(2)",            "u",     eval("x=true; x.__proto__.charAt = String.prototype.charAt; x.charAt(2)") );
new TestCase( SECTION,     "x = true; x.__proto.charAt = String.prototype.charAt; x.charAt(3)",            "e",     eval("x=true; x.__proto__.charAt = String.prototype.charAt; x.charAt(3)") );

new TestCase( SECTION,     "x = NaN; x.__proto.charAt = String.prototype.charAt; x.charAt(0)",            "N",     eval("x=NaN; x.__proto__.charAt = String.prototype.charAt; x.charAt(0)") );
new TestCase( SECTION,     "x = NaN; x.__proto.charAt = String.prototype.charAt; x.charAt(1)",            "a",     eval("x=NaN; x.__proto__.charAt = String.prototype.charAt; x.charAt(1)") );
new TestCase( SECTION,     "x = NaN; x.__proto.charAt = String.prototype.charAt; x.charAt(2)",            "N",     eval("x=NaN; x.__proto__.charAt = String.prototype.charAt; x.charAt(2)") );

new TestCase( SECTION,     "x = 123; x.__proto.charAt = String.prototype.charAt; x.charAt(0)",            "1",     eval("x=123; x.__proto__.charAt = String.prototype.charAt; x.charAt(0)") );
new TestCase( SECTION,     "x = 123; x.__proto.charAt = String.prototype.charAt; x.charAt(1)",            "2",     eval("x=123; x.__proto__.charAt = String.prototype.charAt; x.charAt(1)") );
new TestCase( SECTION,     "x = 123; x.__proto.charAt = String.prototype.charAt; x.charAt(2)",            "3",     eval("x=123; x.__proto__.charAt = String.prototype.charAt; x.charAt(2)") );


test();
