/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.15.js
   ECMA Section:       15.9.5.15
   Description:        Date.prototype.getUTCHours

   1.  Let t be this time value.
   2.  If t is NaN, return NaN.
   3.  Return HourFromTime(t).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5.15";
var TITLE   = "Date.prototype.getUTCHours()";

writeHeaderToLog( SECTION + " "+ TITLE);

addTestCase( TIME_NOW );
addTestCase( TIME_0000 );
addTestCase( TIME_1970 );
addTestCase( TIME_1900 );
addTestCase( TIME_2000 );
addTestCase( UTC_FEB_29_2000 );
addTestCase( UTC_JAN_1_2005 );

new TestCase( "(new Date(NaN)).getUTCHours()",
	      NaN,
	      (new Date(NaN)).getUTCHours() );

new TestCase( "Date.prototype.getUTCHours.length",
	      0,
	      Date.prototype.getUTCHours.length );
test();

function addTestCase( t ) {
  for ( h = 0; h < 24; h+=3 ) {
    t += msPerHour;
    new TestCase( "(new Date("+t+")).getUTCHours()",
		  HourFromTime((t)),
		  (new Date(t)).getUTCHours() );
  }
}
