// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          array_split_1.js
   ECMA Section:       Array.split()
   Description:

   These are tests from free perl suite.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "Free Perl";
var VERSION = "JS1_2";
var TITLE   = "Array.split()";

startTest();

writeHeaderToLog( SECTION + " "+ TITLE);


new TestCase( SECTION,
	      "('a,b,c'.split(',')).length",
	      3,
	      ('a,b,c'.split(',')).length );

new TestCase( SECTION,
	      "('a,b'.split(',')).length",
	      2,
	      ('a,b'.split(',')).length );

new TestCase( SECTION,
	      "('a'.split(',')).length",
	      1,
	      ('a'.split(',')).length );

/*
 * Deviate from ECMA by never splitting an empty string by any separator
 * string into a non-empty array (an array of length 1 that contains the
 * empty string).
 */
new TestCase( SECTION,
	      "(''.split(',')).length",
	      0,
	      (''.split(',')).length );

test();
