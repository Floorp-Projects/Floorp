/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.8.1.3-1.js
   ECMA Section:       15.8.1.3.js
   Description:        All value properties of the Math object should have
   the attributes [DontEnum, DontDelete, ReadOnly]

   this test checks the ReadOnly attribute of Math.LN2

   Author:             christine@netscape.com
   Date:               16 september 1997
*/

var SECTION = "15.8.1.3-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Math.LN2";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "Math.LN2=0; Math.LN2",    
	      0.6931471805599453,    
	      eval("Math.LN2=0; Math.LN2") );

test();
