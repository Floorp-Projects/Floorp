/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

new TestCase( SECTION,     "x = new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(0)",            "1",     eval("x=new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(0)") );
new TestCase( SECTION,     "x = new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(1)",            ",",     eval("x=new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(1)") );
new TestCase( SECTION,     "x = new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(2)",            "2",     eval("x=new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(2)") );
new TestCase( SECTION,     "x = new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(3)",            ",",     eval("x=new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(3)") );
new TestCase( SECTION,     "x = new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(4)",            "3",     eval("x=new Array(1,2,3); x.charAt = String.prototype.charAt; x.charAt(4)") );

new TestCase( SECTION,  "x = new Array(); x.charAt = String.prototype.charAt; x.charAt(0)",                    "",      eval("x = new Array(); x.charAt = String.prototype.charAt; x.charAt(0)") );

new TestCase( SECTION,     "x = new Number(123); x.charAt = String.prototype.charAt; x.charAt(0)",            "1",     eval("x=new Number(123); x.charAt = String.prototype.charAt; x.charAt(0)") );
new TestCase( SECTION,     "x = new Number(123); x.charAt = String.prototype.charAt; x.charAt(1)",            "2",     eval("x=new Number(123); x.charAt = String.prototype.charAt; x.charAt(1)") );
new TestCase( SECTION,     "x = new Number(123); x.charAt = String.prototype.charAt; x.charAt(2)",            "3",     eval("x=new Number(123); x.charAt = String.prototype.charAt; x.charAt(2)") );

new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(0)",            "[",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(0)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(1)",            "o",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(1)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(2)",            "b",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(2)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(3)",            "j",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(3)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(4)",            "e",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(4)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(5)",            "c",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(5)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(6)",            "t",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(6)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(7)",            " ",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(7)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(8)",            "O",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(8)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(9)",            "b",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(9)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(10)",            "j",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(10)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(11)",            "e",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(11)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(12)",            "c",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(12)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(13)",            "t",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(13)") );
new TestCase( SECTION,     "x = new Object(); x.charAt = String.prototype.charAt; x.charAt(14)",            "]",     eval("x=new Object(); x.charAt = String.prototype.charAt; x.charAt(14)") );

new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(0)",            "[",    eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(0)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(1)",            "o",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(1)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(2)",            "b",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(2)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(3)",            "j",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(3)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(4)",            "e",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(4)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(5)",            "c",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(5)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(6)",            "t",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(6)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(7)",            " ",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(7)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(8)",            "F",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(8)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(9)",            "u",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(9)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(10)",            "n",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(10)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(11)",            "c",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(11)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(12)",            "t",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(12)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(13)",            "i",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(13)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(14)",            "o",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(14)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(15)",            "n",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(15)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(16)",            "]",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(16)") );
new TestCase( SECTION,     "x = new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(17)",            "",     eval("x=new Function(); x.toString = Object.prototype.toString; x.charAt = String.prototype.charAt; x.charAt(17)") );


test();
