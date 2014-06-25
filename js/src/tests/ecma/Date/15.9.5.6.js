/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.6.js
   ECMA Section:       15.9.5.6
   Description:        Date.prototype.getFullYear

   1.   Let t be this time value.
   2.   If t is NaN, return NaN.
   3.   Return YearFromTime(LocalTime(t)).
   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5.6";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Date.prototype.getFullYear()";

writeHeaderToLog( SECTION + " "+ TITLE);

addTestCase( TIME_NOW );
addTestCase( TIME_0000 );
addTestCase( TIME_1970 );
addTestCase( TIME_1900 );
addTestCase( TIME_2000 );
addTestCase( UTC_FEB_29_2000 );
addTestCase( UTC_JAN_1_2005 );

new TestCase( SECTION,
	      "(new Date(NaN)).getFullYear()",
	      NaN,
	      (new Date(NaN)).getFullYear() );

new TestCase( SECTION,
	      "Date.prototype.getFullYear.length",
	      0,
	      Date.prototype.getFullYear.length );

test();
function addTestCase( t ) {
  new TestCase( SECTION,
		"(new Date("+t+")).getFullYear()",
		YearFromTime(LocalTime(t)),
		(new Date(t)).getFullYear() );

  new TestCase( SECTION,
		"(new Date("+(t+1)+")).getFullYear()",
		YearFromTime(LocalTime(t+1)),
		(new Date(t+1)).getFullYear() );

  new TestCase( SECTION,
		"(new Date("+(t-1)+")).getFullYear()",
		YearFromTime(LocalTime(t-1)),
		(new Date(t-1)).getFullYear() );

  new TestCase( SECTION,
		"(new Date("+(t-TZ_ADJUST)+")).getFullYear()",
		YearFromTime(LocalTime(t-TZ_ADJUST)),
		(new Date(t-TZ_ADJUST)).getFullYear() );

  new TestCase( SECTION,
		"(new Date("+(t+TZ_ADJUST)+")).getFullYear()",
		YearFromTime(LocalTime(t+TZ_ADJUST)),
		(new Date(t+TZ_ADJUST)).getFullYear() );
}
