/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.23-2.js
   ECMA Section:       15.9.5.23
   Description:        Date.prototype.setTime

   1.  If the this value is not a Date object, generate a runtime error.
   2.  Call ToNumber(time).
   3.  Call TimeClip(Result(1)).
   4.  Set the [[Value]] property of the this value to Result(2).
   5.  Return the value of the [[Value]] property of the this value.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5.23-2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Date.prototype.setTime()";

writeHeaderToLog( SECTION + " "+ TITLE);

test_times = new Array( TIME_NOW, TIME_0000, TIME_1970, TIME_1900, TIME_2000,
			UTC_FEB_29_2000, UTC_JAN_1_2005 );


for ( var j = 0; j < test_times.length; j++ ) {
  addTestCase( new Date(TIME_1970), test_times[j] );
}


new TestCase( SECTION,
	      "(new Date(NaN)).setTime()",
	      NaN,
	      (new Date(NaN)).setTime() );

new TestCase( SECTION,
	      "Date.prototype.setTime.length",
	      1,
	      Date.prototype.setTime.length );
test();

function addTestCase( d, t ) {
  new TestCase( SECTION,
		"( "+d+" ).setTime("+t+")",
		t,
		d.setTime(t) );

  new TestCase( SECTION,
		"( "+d+" ).setTime("+(t+1.1)+")",
		TimeClip(t+1.1),
		d.setTime(t+1.1) );

  new TestCase( SECTION,
		"( "+d+" ).setTime("+(t+1)+")",
		t+1,
		d.setTime(t+1) );

  new TestCase( SECTION,
		"( "+d+" ).setTime("+(t-1)+")",
		t-1,
		d.setTime(t-1) );

  new TestCase( SECTION,
		"( "+d+" ).setTime("+(t-TZ_ADJUST)+")",
		t-TZ_ADJUST,
		d.setTime(t-TZ_ADJUST) );

  new TestCase( SECTION,
		"( "+d+" ).setTime("+(t+TZ_ADJUST)+")",
		t+TZ_ADJUST,
		d.setTime(t+TZ_ADJUST) );
}
