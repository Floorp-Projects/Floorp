/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.5.4.8-1.js
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

   Author:    christine@netscape.com, pschwartau@netscape.com
   Date:      12 November 1997
   Modified:  14 July 2002
   Reason:    See http://bugzilla.mozilla.org/show_bug.cgi?id=155289
   ECMA-262 Ed.3  Section 15.5.4.14
   The length property of the split method is 2
   *
   */

var SECTION = "15.5.4.8-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "String.prototype.split";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,  "String.prototype.split.length",        2,          String.prototype.split.length );
new TestCase( SECTION,  "delete String.prototype.split.length", false,      delete String.prototype.split.length );
new TestCase( SECTION,  "delete String.prototype.split.length; String.prototype.split.length", 2,      eval("delete String.prototype.split.length; String.prototype.split.length") );

// test cases for when split is called with no arguments.

// this is a string object

new TestCase(   SECTION,
		"var s = new String('this is a string object'); typeof s.split()",
		"object",
		eval("var s = new String('this is a string object'); typeof s.split()") );

new TestCase(   SECTION,
		"var s = new String('this is a string object'); Array.prototype.getClass = Object.prototype.toString; (s.split()).getClass()",
		"[object Array]",
		eval("var s = new String('this is a string object'); Array.prototype.getClass = Object.prototype.toString; (s.split()).getClass()") );

new TestCase(   SECTION,
		"var s = new String('this is a string object'); s.split().length",
		1,
		eval("var s = new String('this is a string object'); s.split().length") );

new TestCase(   SECTION,
		"var s = new String('this is a string object'); s.split()[0]",
		"this is a string object",
		eval("var s = new String('this is a string object'); s.split()[0]") );

// this is an object object
new TestCase(   SECTION,
		"var obj = new Object(); obj.split = String.prototype.split; typeof obj.split()",
		"object",
		eval("var obj = new Object(); obj.split = String.prototype.split; typeof obj.split()") );

new TestCase(   SECTION,
		"var obj = new Object(); obj.split = String.prototype.split; Array.prototype.getClass = Object.prototype.toString; obj.getClass()",
		"[object Array]",
		eval("var obj = new Object(); obj.split = String.prototype.split; Array.prototype.getClass = Object.prototype.toString; obj.split().getClass()") );

new TestCase(   SECTION,
		"var obj = new Object(); obj.split = String.prototype.split; obj.split().length",
		1,
		eval("var obj = new Object(); obj.split = String.prototype.split; obj.split().length") );

new TestCase(   SECTION,
		"var obj = new Object(); obj.split = String.prototype.split; obj.split()[0]",
		"[object Object]",
		eval("var obj = new Object(); obj.split = String.prototype.split; obj.split()[0]") );

// this is a function object
new TestCase(   SECTION,
		"var obj = new Function(); obj.split = String.prototype.split; typeof obj.split()",
		"object",
		eval("var obj = new Function(); obj.split = String.prototype.split; typeof obj.split()") );

new TestCase(   SECTION,
		"var obj = new Function(); obj.split = String.prototype.split; Array.prototype.getClass = Object.prototype.toString; obj.getClass()",
		"[object Array]",
		eval("var obj = new Function(); obj.split = String.prototype.split; Array.prototype.getClass = Object.prototype.toString; obj.split().getClass()") );

new TestCase(   SECTION,
		"var obj = new Function(); obj.split = String.prototype.split; obj.split().length",
		1,
		eval("var obj = new Function(); obj.split = String.prototype.split; obj.split().length") );

new TestCase(   SECTION,
		"var obj = new Function(); obj.split = String.prototype.split; obj.toString = Object.prototype.toString; obj.split()[0]",
		"[object Function]",
		eval("var obj = new Function(); obj.split = String.prototype.split; obj.toString = Object.prototype.toString; obj.split()[0]") );

// this is a number object
new TestCase(   SECTION,
		"var obj = new Number(NaN); obj.split = String.prototype.split; typeof obj.split()",
		"object",
		eval("var obj = new Number(NaN); obj.split = String.prototype.split; typeof obj.split()") );

new TestCase(   SECTION,
		"var obj = new Number(Infinity); obj.split = String.prototype.split; Array.prototype.getClass = Object.prototype.toString; obj.getClass()",
		"[object Array]",
		eval("var obj = new Number(Infinity); obj.split = String.prototype.split; Array.prototype.getClass = Object.prototype.toString; obj.split().getClass()") );

new TestCase(   SECTION,
		"var obj = new Number(-1234567890); obj.split = String.prototype.split; obj.split().length",
		1,
		eval("var obj = new Number(-1234567890); obj.split = String.prototype.split; obj.split().length") );

new TestCase(   SECTION,
		"var obj = new Number(-1e21); obj.split = String.prototype.split; obj.split()[0]",
		"-1e+21",
		eval("var obj = new Number(-1e21); obj.split = String.prototype.split; obj.split()[0]") );


// this is the Math object
new TestCase(   SECTION,
		"var obj = Math; obj.split = String.prototype.split; typeof obj.split()",
		"object",
		eval("var obj = Math; obj.split = String.prototype.split; typeof obj.split()") );

new TestCase(   SECTION,
		"var obj = Math; obj.split = String.prototype.split; Array.prototype.getClass = Object.prototype.toString; obj.getClass()",
		"[object Array]",
		eval("var obj = Math; obj.split = String.prototype.split; Array.prototype.getClass = Object.prototype.toString; obj.split().getClass()") );

new TestCase(   SECTION,
		"var obj = Math; obj.split = String.prototype.split; obj.split().length",
		1,
		eval("var obj = Math; obj.split = String.prototype.split; obj.split().length") );

new TestCase(   SECTION,
		"var obj = Math; obj.split = String.prototype.split; obj.split()[0]",
		"[object Math]",
		eval("var obj = Math; obj.split = String.prototype.split; obj.split()[0]") );

// this is an array object
new TestCase(   SECTION,
		"var obj = new Array(1,2,3,4,5); obj.split = String.prototype.split; typeof obj.split()",
		"object",
		eval("var obj = new Array(1,2,3,4,5); obj.split = String.prototype.split; typeof obj.split()") );

new TestCase(   SECTION,
		"var obj = new Array(1,2,3,4,5); obj.split = String.prototype.split; Array.prototype.getClass = Object.prototype.toString; obj.getClass()",
		"[object Array]",
		eval("var obj = new Array(1,2,3,4,5); obj.split = String.prototype.split; Array.prototype.getClass = Object.prototype.toString; obj.split().getClass()") );

new TestCase(   SECTION,
		"var obj = new Array(1,2,3,4,5); obj.split = String.prototype.split; obj.split().length",
		1,
		eval("var obj = new Array(1,2,3,4,5); obj.split = String.prototype.split; obj.split().length") );

new TestCase(   SECTION,
		"var obj = new Array(1,2,3,4,5); obj.split = String.prototype.split; obj.split()[0]",
		"1,2,3,4,5",
		eval("var obj = new Array(1,2,3,4,5); obj.split = String.prototype.split; obj.split()[0]") );

// this is a Boolean object

new TestCase(   SECTION,
		"var obj = new Boolean(); obj.split = String.prototype.split; typeof obj.split()",
		"object",
		eval("var obj = new Boolean(); obj.split = String.prototype.split; typeof obj.split()") );

new TestCase(   SECTION,
		"var obj = new Boolean(); obj.split = String.prototype.split; Array.prototype.getClass = Object.prototype.toString; obj.getClass()",
		"[object Array]",
		eval("var obj = new Boolean(); obj.split = String.prototype.split; Array.prototype.getClass = Object.prototype.toString; obj.split().getClass()") );

new TestCase(   SECTION,
		"var obj = new Boolean(); obj.split = String.prototype.split; obj.split().length",
		1,
		eval("var obj = new Boolean(); obj.split = String.prototype.split; obj.split().length") );

new TestCase(   SECTION,
		"var obj = new Boolean(); obj.split = String.prototype.split; obj.split()[0]",
		"false",
		eval("var obj = new Boolean(); obj.split = String.prototype.split; obj.split()[0]") );


test();
