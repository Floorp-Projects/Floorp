/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.8.2.14.js
   ECMA Section:       15.8.2.14 Math.random()
   returns a number value x with a positive sign
   with 1 > x >= 0 with approximately uniform
   distribution over that range, using an
   implementation-dependent algorithm or strategy.
   This function takes no arguments.

   Description:
   Author:             christine@netscape.com
   Date:               16 september 1997
*/

var SECTION = "15.8.2.14";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Math.random()";

writeHeaderToLog( SECTION + " "+ TITLE);

for ( var item = 0; item < 100; item++ ) {
  var testcase = new TestCase( SECTION, 
			       "Math.random()",   
			       "pass",   
			       null );
  testcase.reason = Math.random();
  testcase.actual = "pass";

  if ( ! ( testcase.reason >= 0) ) {
    testcase.actual = "fail";
  }

  if ( ! (testcase.reason < 1) ) {
    testcase.actual = "fail";
  }
}

test();
