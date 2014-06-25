/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.17.js
   ECMA Section:       15.9.5.17
   Description:        Date.prototype.getUTCMinutes

   1.  Let t be this time value.
   2.  If t is NaN, return NaN.
   3.  Return MinFromTime(t).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5.17";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Date.prototype.getUTCMinutes()";

writeHeaderToLog( SECTION + " "+ TITLE);

addTestCase( TIME_NOW );
addTestCase( TIME_0000 );
addTestCase( TIME_1970 );
addTestCase( TIME_1900 );
addTestCase( TIME_2000 );
addTestCase( UTC_FEB_29_2000 );
addTestCase( UTC_JAN_1_2005 );

new TestCase( SECTION,
	      "(new Date(NaN)).getUTCMinutes()",
	      NaN,
	      (new Date(NaN)).getUTCMinutes() );

new TestCase( SECTION,
	      "Date.prototype.getUTCMinutes.length",
	      0,
	      Date.prototype.getUTCMinutes.length );
test();

function addTestCase( t ) {
  for ( m = 0; m <= 60; m+=10 ) {
    t += msPerMinute;
    new TestCase( SECTION,
		  "(new Date("+t+")).getUTCMinutes()",
		  MinFromTime(t),
		  (new Date(t)).getUTCMinutes() );
  }
}
