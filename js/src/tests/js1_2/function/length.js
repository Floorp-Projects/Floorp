// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.3.5.1.js
   ECMA Section:       Function.length
   Description:

   The value of the length property is usually an integer that indicates the
   "typical" number of arguments expected by the function.  However, the
   language permits the function to be invoked with some other number of
   arguments. The behavior of a function when invoked on a number of arguments
   other than the number specified by its length property depends on the function.

   This checks the pre-ecma behavior Function.length.

   http://scopus.mcom.com/bugsplat/show_bug.cgi?id=104204


   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "function/length.js";
var VERSION = "ECMA_1";
var TITLE   = "Function.length";
var BUGNUMBER="104204";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

var f = new Function( "a","b", "c", "return f.length");

if ( version() <= 120 ) {

  new TestCase( SECTION,
		'var f = new Function( "a","b", "c", "return f.length"); f()',
		0,
		f() );

  new TestCase( SECTION,
		'var f = new Function( "a","b", "c", "return f.length"); f(1,2,3,4,5)',
		5,
		f(1,2,3,4,5) );
} else {

  new TestCase( SECTION,
		'var f = new Function( "a","b", "c", "return f.length"); f()',
		3,
		f() );

  new TestCase( SECTION,
		'var f = new Function( "a","b", "c", "return f.length"); f(1,2,3,4,5)',
		3,
		f(1,2,3,4,5) );


}
test();
