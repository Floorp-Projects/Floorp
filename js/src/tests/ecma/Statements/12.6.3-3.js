/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          12.6.3-3.js
   ECMA Section:       for..in loops
   Description:

   This verifies the fix to
   http://scopus.mcom.com/bugsplat/show_bug.cgi?id=112156
   for..in should take general lvalue for first argument

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "12.6.3-3";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The for..in statement";

writeHeaderToLog( SECTION + " "+ TITLE);

var o = {};

var result = "";

for ( o.a in [1,2,3] ) { result += String( [1,2,3][o.a] ); }

new TestCase( SECTION,
	      "for ( o.a in [1,2,3] ) { result += String( [1,2,3][o.a] ); } result",
	      "123",
	      result );

test();

