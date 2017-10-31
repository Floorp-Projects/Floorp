/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.7.js
   ECMA Section:       15.9.5.7
   Description:        Date.prototype.getUTCFullYear

   1.Let t be this time value.
   2.If t is NaN, return NaN.
   3.Return YearFromTime(t).
   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5.7";
var TITLE   = "Date.prototype.getUTCFullYear()";

writeHeaderToLog( SECTION + " "+ TITLE);

addTestCase( TIME_NOW );
addTestCase( TIME_0000 );
addTestCase( TIME_1970 );
addTestCase( TIME_1900 );
addTestCase( TIME_2000 );
addTestCase( UTC_FEB_29_2000 );
addTestCase( UTC_JAN_1_2005 );

new TestCase( "(new Date(NaN)).getUTCFullYear()",
	      NaN,
	      (new Date(NaN)).getUTCFullYear() );

new TestCase( "Date.prototype.getUTCFullYear.length",
	      0,
	      Date.prototype.getUTCFullYear.length );

test();

function addTestCase( t ) {
  new TestCase( "(new Date("+t+")).getUTCFullYear()",
		YearFromTime(t),
		(new Date(t)).getUTCFullYear() );

  new TestCase( "(new Date("+(t+1)+")).getUTCFullYear()",
		YearFromTime(t+1),
		(new Date(t+1)).getUTCFullYear() );

  new TestCase( "(new Date("+(t-1)+")).getUTCFullYear()",
		YearFromTime(t-1),
		(new Date(t-1)).getUTCFullYear() );

  new TestCase( "(new Date("+(t-TZ_ADJUST)+")).getUTCFullYear()",
		YearFromTime(t-TZ_ADJUST),
		(new Date(t-TZ_ADJUST)).getUTCFullYear() );

  new TestCase( "(new Date("+(t+TZ_ADJUST)+")).getUTCFullYear()",
		YearFromTime(t+TZ_ADJUST),
		(new Date(t+TZ_ADJUST)).getUTCFullYear() );
}
