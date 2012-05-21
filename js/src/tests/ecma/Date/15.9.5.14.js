// |reftest| random
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.14.js
   ECMA Section:       15.9.5.14
   Description:        Date.prototype.getHours
   1.  Let t be this time value.
   2.  If t is NaN, return NaN.
   3.  Return HourFromTime(LocalTime(t)).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5.14";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Date.prototype.getHours()";

writeHeaderToLog( SECTION + " "+ TITLE);

addTestCase( TIME_NOW );
addTestCase( TIME_0000 );
addTestCase( TIME_1970 );
addTestCase( TIME_1900 );
addTestCase( TIME_2000 );
addTestCase( UTC_FEB_29_2000 );
addTestCase( UTC_JAN_1_2005 );

new TestCase( SECTION,
	      "(new Date(NaN)).getHours()",
	      NaN,
	      (new Date(NaN)).getHours() );

new TestCase( SECTION,
	      "Date.prototype.getHours.length",
	      0,
	      Date.prototype.getHours.length );
test();

function addTestCase( t ) {
  for ( h = 0; h < 24; h+=4 ) {
    t += msPerHour;
    new TestCase( SECTION,
		  "(new Date("+t+")).getHours()",
		  HourFromTime((LocalTime(t))),
		  (new Date(t)).getHours() );
  }
}
