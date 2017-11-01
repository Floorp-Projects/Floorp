/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.1.1-1.js
   ECMA Section:       15.9.1.1 Time Range
   Description:
   - leap seconds are ignored
   - assume 86400000 ms / day
   - numbers range fom +/- 9,007,199,254,740,991
   - ms precision for any instant that is within
   approximately +/-285,616 years from 1 jan 1970
   UTC
   - range of times supported is -100,000,000 days
   to 100,000,000 days from 1 jan 1970 12:00 am
   - time supported is 8.64e5*10e8 milliseconds from
   1 jan 1970 UTC  (+/-273972.6027397 years)

   -   this test generates its own data -- it does not
   read data from a file.
   Author:             christine@netscape.com
   Date:               7 july 1997

   Static variables:
   FOUR_HUNDRED_YEARS

*/

//  every one hundred years contains:
//    24 years with 366 days
//
//  every four hundred years contains:
//    97 years with 366 days
//   303 years with 365 days
//
//   86400000*365*97    =    3067372800000
//  +86400000*366*303   =  + 9555408000000
//                      =    1.26227808e+13
var FOUR_HUNDRED_YEARS = 1.26227808e+13;
var SECTION         =  "15.9.1.1-1";

writeHeaderToLog("15.9.1.1 Time Range");

var M_SECS;
var CURRENT_YEAR;

for ( M_SECS = 0, CURRENT_YEAR = 1970;
      M_SECS < 8640000000000000;
      M_SECS += FOUR_HUNDRED_YEARS, CURRENT_YEAR += 400 ) {

  new TestCase(
		"new Date("+M_SECS+")",
		CURRENT_YEAR,
		(new Date( M_SECS)).getUTCFullYear() );
}

test();

