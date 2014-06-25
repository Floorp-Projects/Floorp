/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.22.js
   ECMA Section:       15.9.5.22
   Description:        Date.prototype.getTimezoneOffset

   Returns the difference between local time and UTC time in minutes.
   1.  Let t be this time value.
   2.  If t is NaN, return NaN.
   3.  Return (t - LocalTime(t)) / msPerMinute.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5.22";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Date.prototype.getTimezoneOffset()";

writeHeaderToLog( SECTION + " "+ TITLE);

addTestCase( UTC_JAN_1_2005 );

test();

function addTestCase( t ) {
  for ( m = 0; m <= 1000; m+=100 ) {
    t++;
    new TestCase( SECTION,
		  "(new Date("+t+")).getTimezoneOffset()",
		  (t - LocalTime(t)) / msPerMinute,
		  (new Date(t)).getTimezoneOffset() );
  }
}
