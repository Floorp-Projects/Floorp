/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.12
   ECMA Section:       15.9.5.12
   Description:        Date.prototype.getDay


   1.   Let t be this time value.
   2.   If t is NaN, return NaN.
   3.   Return WeekDay(LocalTime(t)).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5.12";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Date.prototype.getDay()";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "(new Date(NaN)).getDay()",
	      NaN,
	      (new Date(NaN)).getDay() );

new TestCase( SECTION,
	      "Date.prototype.getDay.length",
	      0,
	      Date.prototype.getDay.length );
test();
