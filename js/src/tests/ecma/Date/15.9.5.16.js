// |reftest| skip-if(!xulRuntime.shell&&xulRuntime.OS=="Linux"&&xulRuntime.XPCOMABI.match(/x86_64/)) -- bug xxx crash
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.16.js
   ECMA Section:       15.9.5.16
   Description:        Date.prototype.getMinutes
   1.Let t be this time value.
   2.If t is NaN, return NaN.
   3.Return MinFromTime(LocalTime(t)).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5.16";
var TITLE   = "Date.prototype.getMinutes()";

writeHeaderToLog( SECTION + " "+ TITLE);

addTestCase( TIME_NOW );
addTestCase( TIME_0000 );
addTestCase( TIME_1970 );
addTestCase( TIME_1900 );
addTestCase( TIME_2000 );
addTestCase( UTC_FEB_29_2000 );
addTestCase( UTC_JAN_1_2005 );

new TestCase( "(new Date(NaN)).getMinutes()",
	      NaN,
	      (new Date(NaN)).getMinutes() );

new TestCase( "Date.prototype.getMinutes.length",
	      0,
	      Date.prototype.getMinutes.length );
test();

function addTestCase( t ) {
  for ( m = 0; m <= 60; m+=10 ) {
    t += msPerMinute;
    new TestCase( "(new Date("+t+")).getMinutes()",
		  MinFromTime((LocalTime(t))),
		  (new Date(t)).getMinutes() );
  }
}
