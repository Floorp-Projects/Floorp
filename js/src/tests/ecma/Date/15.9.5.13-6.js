/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.13.js
   ECMA Section:       15.9.5.13
   Description:        Date.prototype.getUTCDay

   1.Let t be this time value.
   2.If t is NaN, return NaN.
   3.Return WeekDay(t).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5.13";
var TITLE   = "Date.prototype.getUTCDay()";

writeHeaderToLog( SECTION + " "+ TITLE);

addTestCase( UTC_FEB_29_2000 );

test();

function addTestCase( t ) {
  var start = TimeFromYear(YearFromTime(t));
  var stop  = TimeFromYear(YearFromTime(t) + 1);

  for (var d = start; d < stop; d += msPerDay)
  {
    new TestCase( "(new Date("+d+")).getUTCDay()",
                  WeekDay((d)),
                  (new Date(d)).getUTCDay() );
  }
}

