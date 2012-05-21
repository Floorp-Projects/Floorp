/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.5.4.10-1.js
   ECMA Section:       15.5.4.10 String.prototype.substring( start, end )
   Description:

   15.5.4.10 String.prototype.substring(start, end)

   Returns a substring of the result of converting this object to a string,
   starting from character position start and running to character position
   end of the string. The result is a string value, not a String object.

   If either argument is NaN or negative, it is replaced with zero; if either
   argument is larger than the length of the string, it is replaced with the
   length of the string.

   If start is larger than end, they are swapped.

   When the substring method is called with two arguments start and end, the
   following steps are taken:

   1.  Call ToString, giving it the this value as its argument.
   2.  Call ToInteger(start).
   3.  Call ToInteger (end).
   4.  Compute the number of characters in Result(1).
   5.  Compute min(max(Result(2), 0), Result(4)).
   6.  Compute min(max(Result(3), 0), Result(4)).
   7.  Compute min(Result(5), Result(6)).
   8.  Compute max(Result(5), Result(6)).
   9.  Return a string whose length is the difference between Result(8) and
   Result(7), containing characters from Result(1), namely the characters
   with indices Result(7) through Result(8)1, in ascending order.

   Note that the substring function is intentionally generic; it does not require
   that its this value be a String object. Therefore it can be transferred to other
   kinds of objects for use as a method.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.5.4.10-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "String.prototype.substring( start, end )";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,  "String.prototype.substring.length",        2,          String.prototype.substring.length );
new TestCase( SECTION,  "delete String.prototype.substring.length", false,      delete String.prototype.substring.length );
new TestCase( SECTION,  "delete String.prototype.substring.length; String.prototype.substring.length", 2,      eval("delete String.prototype.substring.length; String.prototype.substring.length") );

// test cases for when substring is called with no arguments.

// this is a string object

new TestCase(   SECTION,
		"var s = new String('this is a string object'); typeof s.substring()",
		"string",
		eval("var s = new String('this is a string object'); typeof s.substring()") );

new TestCase(   SECTION,
		"var s = new String(''); s.substring(1,0)",
		"",
		eval("var s = new String(''); s.substring(1,0)") );

new TestCase(   SECTION,
		"var s = new String('this is a string object'); s.substring(true, false)",
		"t",
		eval("var s = new String('this is a string object'); s.substring(false, true)") );

new TestCase(   SECTION,
		"var s = new String('this is a string object'); s.substring(NaN, Infinity)",
		"this is a string object",
		eval("var s = new String('this is a string object'); s.substring(NaN, Infinity)") );


new TestCase(   SECTION,
		"var s = new String('this is a string object'); s.substring(Infinity, NaN)",
		"this is a string object",
		eval("var s = new String('this is a string object'); s.substring(Infinity, NaN)") );


new TestCase(   SECTION,
		"var s = new String('this is a string object'); s.substring(Infinity, Infinity)",
		"",
		eval("var s = new String('this is a string object'); s.substring(Infinity, Infinity)") );

new TestCase(   SECTION,
		"var s = new String('this is a string object'); s.substring(-0.01, 0)",
		"",
		eval("var s = new String('this is a string object'); s.substring(-0.01,0)") );


new TestCase(   SECTION,
		"var s = new String('this is a string object'); s.substring(s.length, s.length)",
		"",
		eval("var s = new String('this is a string object'); s.substring(s.length, s.length)") );

new TestCase(   SECTION,
		"var s = new String('this is a string object'); s.substring(s.length+1, 0)",
		"this is a string object",
		eval("var s = new String('this is a string object'); s.substring(s.length+1, 0)") );


new TestCase(   SECTION,
		"var s = new String('this is a string object'); s.substring(-Infinity, -Infinity)",
		"",
		eval("var s = new String('this is a string object'); s.substring(-Infinity, -Infinity)") );

// this is not a String object, start is not an integer


new TestCase(   SECTION,
		"var s = new Array(1,2,3,4,5); s.substring = String.prototype.substring; s.substring(Infinity,-Infinity)",
		"1,2,3,4,5",
		eval("var s = new Array(1,2,3,4,5); s.substring = String.prototype.substring; s.substring(Infinity,-Infinity)") );

new TestCase(   SECTION,
		"var s = new Array(1,2,3,4,5); s.substring = String.prototype.substring; s.substring(true, false)",
		"1",
		eval("var s = new Array(1,2,3,4,5); s.substring = String.prototype.substring; s.substring(true, false)") );

new TestCase(   SECTION,
		"var s = new Array(1,2,3,4,5); s.substring = String.prototype.substring; s.substring('4', '5')",
		"3",
		eval("var s = new Array(1,2,3,4,5); s.substring = String.prototype.substring; s.substring('4', '5')") );


// this is an object object
new TestCase(   SECTION,
		"var obj = new Object(); obj.substring = String.prototype.substring; obj.substring(8,0)",
		"[object ",
		eval("var obj = new Object(); obj.substring = String.prototype.substring; obj.substring(8,0)") );

new TestCase(   SECTION,
		"var obj = new Object(); obj.substring = String.prototype.substring; obj.substring(8,obj.toString().length)",
		"Object]",
		eval("var obj = new Object(); obj.substring = String.prototype.substring; obj.substring(8, obj.toString().length)") );

// this is a function object
new TestCase(   SECTION,
		"var obj = new Function(); obj.substring = String.prototype.substring; obj.toString = Object.prototype.toString; obj.substring(8, Infinity)",
		"Function]",
		eval("var obj = new Function(); obj.substring = String.prototype.substring; obj.toString = Object.prototype.toString; obj.substring(8,Infinity)") );
// this is a number object
new TestCase(   SECTION,
		"var obj = new Number(NaN); obj.substring = String.prototype.substring; obj.substring(Infinity, NaN)",
		"NaN",
		eval("var obj = new Number(NaN); obj.substring = String.prototype.substring; obj.substring(Infinity, NaN)") );

// this is the Math object
new TestCase(   SECTION,
		"var obj = Math; obj.substring = String.prototype.substring; obj.substring(Math.PI, -10)",
		"[ob",
		eval("var obj = Math; obj.substring = String.prototype.substring; obj.substring(Math.PI, -10)") );

// this is a Boolean object

new TestCase(   SECTION,
		"var obj = new Boolean(); obj.substring = String.prototype.substring; obj.substring(new Array(), new Boolean(1))",
		"f",
		eval("var obj = new Boolean(); obj.substring = String.prototype.substring; obj.substring(new Array(), new Boolean(1))") );

// this is a user defined object

new TestCase( SECTION,
	      "var obj = new MyObject( void 0 ); obj.substring(0, 100)",
	      "undefined",
	      eval( "var obj = new MyObject( void 0 ); obj.substring(0,100)") );

test();

function MyObject( value ) {
  this.value = value;
  this.substring = String.prototype.substring;
  this.toString = new Function ( "return this.value+''" );
}
