// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          tostring_1.js
   ECMA Section:       Array.toString()
   Description:

   This checks the ToString value of Array objects under JavaScript 1.2.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "JS1_2";
var VERSION = "JS1_2";
startTest();
var TITLE   = "Array.toString()";

writeHeaderToLog( SECTION + " "+ TITLE);

var a = new Array();

var VERSION = 0;

if ( version() == 120 ) {
  VERSION = "120";
} else {
  VERSION = "";
}

new TestCase ( SECTION,
	       "var a = new Array(); a.toString()",
	       ( VERSION == "120" ? "[]" : "" ),
	       a.toString() );

a[0] = void 0;

new TestCase ( SECTION,
	       "a[0] = void 0; a.toString()",
	       ( VERSION == "120" ? "[, ]" : "" ),
	       a.toString() );


new TestCase( SECTION,
	      "a.length",
	      1,
	      a.length );

a[1] = void 0;

new TestCase( SECTION,
	      "a[1] = void 0; a.toString()",
	      ( VERSION == "120" ? "[, , ]" : ","  ),
	      a.toString() );

a[1] = "hi";

new TestCase( SECTION,
	      "a[1] = \"hi\"; a.toString()",
	      ( VERSION == "120" ? "[, \"hi\"]" : ",hi" ),
	      a.toString() );

a[2] = void 0;

new TestCase( SECTION,
	      "a[2] = void 0; a.toString()",
	      ( VERSION == "120" ?"[, \"hi\", , ]":",hi,"),
	      a.toString() );

var b = new Array(1000);
var bstring = "";
for ( blen=0; blen<999; blen++) {
  bstring += ",";
}


new TestCase ( SECTION,
	       "var b = new Array(1000); b.toString()",
	       ( VERSION == "120" ? "[1000]" : bstring ),
	       b.toString() );


new TestCase( SECTION,
	      "b.length",
	      ( VERSION == "120" ? 1 : 1000 ),
	      b.length );

test();
