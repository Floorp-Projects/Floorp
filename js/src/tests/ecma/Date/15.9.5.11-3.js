/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.11.js
   ECMA Section:       15.9.5.11
   Description:        Date.prototype.getUTCDate

   1.Let t be this time value.
   2.If t is NaN, return NaN.
   1.Return DateFromTime(t).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5.11";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Date.prototype.getUTCDate()";

writeHeaderToLog( SECTION + " "+ TITLE);

addTestCase( TIME_1970 );

test();

function addTestCase( t ) {
  var start = TimeFromYear(YearFromTime(t));
  var stop  = TimeFromYear(YearFromTime(t) + 1);

  for (var d = start; d < stop; d += msPerDay)
  {
    new TestCase( SECTION,
                  "(new Date("+d+")).getUTCDate()",
                  DateFromTime(d),
                  (new Date(d)).getUTCDate() );
  }
}
