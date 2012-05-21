/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.4.1.3.js
   ECMA Section:       15.4.1.3 Array()

   Description:        When Array is called as a function rather than as a
   constructor, it creates and initializes a new array
   object.  Thus, the function call Array(...) is
   equivalent to the object creationi new Array(...) with
   the same arguments.

   An array is created and returned as if by the
   expression new Array(len).

   Author:             christine@netscape.com
   Date:               7 october 1997
*/
var SECTION = "15.4.1.3";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Array Constructor Called as a Function:  Array()";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase(   SECTION,
		"typeof Array()",
		"object",
		typeof Array() );

new TestCase(   SECTION,
		"MYARR = new Array();MYARR.getClass = Object.prototype.toString;MYARR.getClass()",
		"[object Array]",
		eval("MYARR = Array();MYARR.getClass = Object.prototype.toString;MYARR.getClass()") );

new TestCase(   SECTION,
		"(Array()).length",
		0,         
		(Array()).length );

new TestCase(   SECTION,
		"Array().toString()",
		"",
		Array().toString() );

test();
