/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.10.js
   ECMA Section:       15.9.5.10
   Description:        Date.prototype.getDate

   1.Let t be this time value.
   2.If t is NaN, return NaN.
   3.Return DateFromTime(LocalTime(t)).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5.10";
var TITLE   = "Date.prototype.getDate()";

writeHeaderToLog( SECTION + " "+ TITLE);

addTestCase( TIME_2000 );

new TestCase( "(new Date(NaN)).getDate()",
              NaN,
              (new Date(NaN)).getDate() );

new TestCase( "Date.prototype.getDate.length",
              0,
              Date.prototype.getDate.length );
test();

function addTestCase( t ) {
  var start = TimeFromYear(YearFromTime(t));
  var stop  = TimeFromYear(YearFromTime(t) + 1);

  for (var d = start; d < stop; d += msPerDay)
  {
    new TestCase( "(new Date("+d+")).getDate()",
                  DateFromTime(LocalTime(d)),
                  (new Date(d)).getDate() );
  }
}
