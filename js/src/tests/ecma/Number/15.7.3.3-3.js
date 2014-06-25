/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.7.3.3-3.js
   ECMA Section:       15.7.3.3 Number.MIN_VALUE
   Description:        All value properties of the Number object should have
   the attributes [DontEnum, DontDelete, ReadOnly]

   this test checks the ReadOnly attribute of Number.MIN_VALUE

   Author:             christine@netscape.com
   Date:               16 september 1997
*/
var SECTION = "15.7.3.3-3";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Number.MIN_VALUE:  ReadOnly Attribute";

writeHeaderToLog( SECTION + " "+TITLE );

new TestCase( SECTION,
	      "Number.MIN_VALUE=0; Number.MIN_VALUE",
	      Number.MIN_VALUE,
	      eval("Number.MIN_VALUE=0; Number.MIN_VALUE" ));

test();
