/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.7.3.4-4.js
   ECMA Section:       15.7.3.4 Number.NaN
   Description:        All value properties of the Number object should have
   the attributes [DontEnum, DontDelete, ReadOnly]

   this test checks the DontEnum attribute of Number.NaN

   Author:             christine@netscape.com
   Date:               16 september 1997
*/

var SECTION = "15.7.3.4-4";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Number.NaN";

writeHeaderToLog( SECTION + " " + TITLE);

new TestCase( SECTION,
	      "var string = ''; for ( prop in Number ) { string += ( prop == 'NaN' ) ? prop : '' } string;",
	      "",
	      eval("var string = ''; for ( prop in Number ) { string += ( prop == 'NaN' ) ? prop : '' } string;")
  );

test();
