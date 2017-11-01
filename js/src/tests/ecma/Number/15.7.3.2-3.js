/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.7.3.2-3.js
   ECMA Section:       15.7.3.2 Number.MAX_VALUE
   Description:        All value properties of the Number object should have
   the attributes [DontEnum, DontDelete, ReadOnly]

   this test checks the ReadOnly attribute of Number.MAX_VALUE

   Author:             christine@netscape.com
   Date:               16 september 1997
*/

var SECTION = "15.7.3.2-3";
var TITLE   = "Number.MAX_VALUE";

writeHeaderToLog( SECTION + " "+ TITLE );

var MAX_VAL = 1.7976931348623157e308;

new TestCase( "Number.MAX_VALUE=0; Number.MAX_VALUE",
	      MAX_VAL,
	      eval("Number.MAX_VALUE=0; Number.MAX_VALUE") );

test();
