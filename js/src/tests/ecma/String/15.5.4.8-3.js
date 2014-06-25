/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.5.4.8-3.js
   ECMA Section:       15.5.4.8 String.prototype.split( separator )
   Description:

   Returns an Array object into which substrings of the result of converting
   this object to a string have been stored. The substrings are determined by
   searching from left to right for occurrences of the given separator; these
   occurrences are not part of any substring in the returned array, but serve
   to divide up this string value. The separator may be a string of any length.

   As a special case, if the separator is the empty string, the string is split
   up into individual characters; the length of the result array equals the
   length of the string, and each substring contains one character.

   If the separator is not supplied, then the result array contains just one
   string, which is the string.

   When the split method is called with one argument separator, the following steps are taken:

   1.   Call ToString, giving it the this value as its argument.
   2.   Create a new Array object of length 0 and call it A.
   3.   If separator is not supplied, call the [[Put]] method of A with 0 and
   Result(1) as arguments, and then return A.
   4.   Call ToString(separator).
   5.   Compute the number of characters in Result(1).
   6.   Compute the number of characters in the string that is Result(4).
   7.   Let p be 0.
   8.   If Result(6) is zero (the separator string is empty), go to step 17.
   9.   Compute the smallest possible integer k not smaller than p such that
   k+Result(6) is not greater than Result(5), and for all nonnegative
   integers j less than Result(6), the character at position k+j of
   Result(1) is the same as the character at position j of Result(2);
   but if there is no such integer k, then go to step 14.
   10.   Compute a string value equal to the substring of Result(1), consisting
   of the characters at positions p through k1, inclusive.
   11.   Call the [[Put]] method of A with A.length and Result(10) as arguments.
   12.   Let p be k+Result(6).
   13.   Go to step 9.
   14.   Compute a string value equal to the substring of Result(1), consisting
   of the characters from position p to the end of Result(1).
   15.   Call the [[Put]] method of A with A.length and Result(14) as arguments.
   16.   Return A.
   17.   If p equals Result(5), return A.
   18.   Compute a string value equal to the substring of Result(1), consisting of
   the single character at position p.
   19.   Call the [[Put]] method of A with A.length and Result(18) as arguments.
   20.   Increase p by 1.
   21.   Go to step 17.

   Note that the split function is intentionally generic; it does not require that its this value be a String
   object. Therefore it can be transferred to other kinds of objects for use as a method.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.5.4.8-3";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "String.prototype.split";

writeHeaderToLog( SECTION + " "+ TITLE);

var TEST_STRING = "";
var EXPECT = new Array();

// this.toString is the empty string.

new TestCase(   SECTION,
		"var s = new String(); s.split().length",
		1,
		eval("var s = new String(); s.split().length") );

new TestCase(   SECTION,
		"var s = new String(); s.split()[0]",
		"",
		eval("var s = new String(); s.split()[0]") );

// this.toString() is the empty string, separator is specified.

new TestCase(   SECTION,
		"var s = new String(); s.split('').length",
		0,
		eval("var s = new String(); s.split('').length") );

new TestCase(   SECTION,
		"var s = new String(); s.split(' ').length",
		1,
		eval("var s = new String(); s.split(' ').length") );

// this to string is " "
new TestCase(   SECTION,
		"var s = new String(' '); s.split().length",
		1,
		eval("var s = new String(' '); s.split().length") );

new TestCase(   SECTION,
		"var s = new String(' '); s.split()[0]",
		" ",
		eval("var s = new String(' '); s.split()[0]") );

new TestCase(   SECTION,
		"var s = new String(' '); s.split('').length",
		1,
		eval("var s = new String(' '); s.split('').length") );

new TestCase(   SECTION,
		"var s = new String(' '); s.split('')[0]",
		" ",
		eval("var s = new String(' '); s.split('')[0]") );

new TestCase(   SECTION,
		"var s = new String(' '); s.split(' ').length",
		2,
		eval("var s = new String(' '); s.split(' ').length") );

new TestCase(   SECTION,
		"var s = new String(' '); s.split(' ')[0]",
		"",
		eval("var s = new String(' '); s.split(' ')[0]") );

new TestCase(   SECTION,
		"\"\".split(\"\").length",
		0,
		("".split("")).length );

new TestCase(   SECTION,
		"\"\".split(\"x\").length",
		1,
		("".split("x")).length );

new TestCase(   SECTION,
		"\"\".split(\"x\")[0]",
		"",
		("".split("x"))[0] );

test();

function Split( string, separator ) {
  string = String( string );

  var A = new Array();

  if ( arguments.length < 2 ) {
    A[0] = string;
    return A;
  }

  separator = String( separator );

  var str_len = String( string ).length;
  var sep_len = String( separator ).length;

  var p = 0;
  var k = 0;

  if ( sep_len == 0 ) {
    for ( ; p < str_len; p++ ) {
      A[A.length] = String( string.charAt(p) );
    }
  }
  return A;
}
