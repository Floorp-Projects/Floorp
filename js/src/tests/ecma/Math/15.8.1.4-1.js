/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.8.1.4-1.js
   ECMA Section:       15.8.1.4.js
   Description:        All value properties of the Math object should have
   the attributes [DontEnum, DontDelete, ReadOnly]

   this test checks the ReadOnly attribute of Math.LOG2E

   Author:             christine@netscape.com
   Date:               16 september 1997
*/

var SECTION = "15.8.1.4-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Math.LOG2E";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "Math.L0G2E=0; Math.LOG2E",
	      1.4426950408889634,    
	      eval("Math.LOG2E=0; Math.LOG2E") );

test();
