/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.8.1.1-1.js
   ECMA Section:       15.8.1.1.js
   Description:        All value properties of the Math object should have
   the attributes [DontEnum, DontDelete, ReadOnly]

   this test checks the ReadOnly attribute of Math.E

   Author:             christine@netscape.com
   Date:               16 september 1997
*/
var SECTION = "15.8.1.1-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Math.E";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "Math.E = 0; Math.E",      
	      2.7182818284590452354, 
	      eval("Math.E=0;Math.E") );

test();
