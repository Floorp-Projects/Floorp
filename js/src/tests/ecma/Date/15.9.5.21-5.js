/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.21.js
   ECMA Section:       15.9.5.21
   Description:        Date.prototype.getUTCMilliseconds

   1.  Let t be this time value.
   2.  If t is NaN, return NaN.
   3.  Return msFromTime(t).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5.21";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Date.prototype.getUTCMilliseconds()";

writeHeaderToLog( SECTION + " "+ TITLE);

addTestCase( TIME_2000 );

test();

function addTestCase( t ) {
  new TestCase( SECTION,
		"(new Date("+t+")).getUTCMilliseconds()",
		msFromTime(t),
		(new Date(t)).getUTCMilliseconds() );
}
