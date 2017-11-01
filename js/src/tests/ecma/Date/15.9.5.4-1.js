/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.4-1.js
   ECMA Section:       15.9.5.4-1 Date.prototype.getTime
   Description:

   1.  If the this value is not an object whose [[Class]] property is "Date",
   generate a runtime error.
   2.  Return this time value.
   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "15.9.5.4-1";
var TITLE   = "Date.prototype.getTime";

writeHeaderToLog( SECTION + " "+ TITLE);

addTestCase( TIME_NOW );
addTestCase( TIME_1970 );
addTestCase( TIME_1900 );
addTestCase( TIME_2000 );
addTestCase( UTC_FEB_29_2000 );
addTestCase( UTC_JAN_1_2005 );

test();

function addTestCase( t ) {
  new TestCase( "(new Date("+t+").getTime()",
		t,
		(new Date(t)).getTime() );

  new TestCase( "(new Date("+(t+1)+").getTime()",
		t+1,
		(new Date(t+1)).getTime() );

  new TestCase( "(new Date("+(t-1)+").getTime()",
		t-1,
		(new Date(t-1)).getTime() );

  new TestCase( "(new Date("+(t-TZ_ADJUST)+").getTime()",
		t-TZ_ADJUST,
		(new Date(t-TZ_ADJUST)).getTime() );

  new TestCase( "(new Date("+(t+TZ_ADJUST)+").getTime()",
		t+TZ_ADJUST,
		(new Date(t+TZ_ADJUST)).getTime() );
}
