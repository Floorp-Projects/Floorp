/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:     delete-001.js
   Section:       regress
   Description:

   Regression test for
   http://scopus.mcom.com/bugsplat/show_bug.cgi?id=108736

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "JS1_2";
var VERSION = "JS1_2";
var TITLE   = "The variable statement";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

// delete all properties of the global object
// per ecma, this does not affect variables in the global object declared
// with var or functions

for ( p in this ) {
  delete p;
}

var result ="";

for ( p in this ) {
  result += String( p );
}

// not too picky here... just want to make sure we didn't crash or something

new TestCase( SECTION,
	      "delete all properties of the global object",
	      "PASSED",
	      result == "" ? "FAILED" : "PASSED" );


test();

