/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          7.7.3-2.js
   ECMA Section:       7.7.3 Numeric Literals

   Description:

   This is a regression test for
   http://scopus.mcom.com/bugsplat/show_bug.cgi?id=122884

   Waldemar's comments:

   A numeric literal that starts with either '08' or '09' is interpreted as a
   decimal literal; it should be an error instead.  (Strictly speaking, according
   to ECMA v1 such literals should be interpreted as two integers -- a zero
   followed by a decimal number whose first digit is 8 or 9, but this is a bug in
   ECMA that will be fixed in v2.  In any case, there is no place in the grammar
   where two consecutive numbers would be legal.)

   Author:             christine@netscape.com
   Date:               15 june 1998

*/
var SECTION = "7.7.3-2";
var TITLE   = "Numeric Literals";
var BUGNUMBER="122884";

printBugNumber(BUGNUMBER);

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "9",
	      9,
	      9 );

new TestCase( "09",
	      9,
	      09 );

new TestCase( "099",
	      99,
	      099 );


new TestCase( "077",
	      63,
	      077 );

test();
