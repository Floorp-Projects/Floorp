/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.8.1.3-3.js
   ECMA Section:       15.8.1.3.js
   Description:        All value properties of the Math object should have
   the attributes [DontEnum, DontDelete, ReadOnly]

   this test checks the DontDelete attribute of Math.LN2

   Author:             christine@netscape.com
   Date:               16 september 1997
*/

var SECTION = "15.8.1.3-2";
var TITLE   = "Math.LN2";

writeHeaderToLog( SECTION + " "+ TITLE);

var MATH_LN2 = 0.6931471805599453;

new TestCase( "delete(Math.LN2)",             
	      false,         
	      eval("delete(Math.LN2)") );

new TestCase( "delete(Math.LN2); Math.LN2",   
	      MATH_LN2,      
	      eval("delete(Math.LN2); Math.LN2") );

test();
