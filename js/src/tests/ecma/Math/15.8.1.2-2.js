/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.8.1.2-1.js
   ECMA Section:       15.8.2.js
   Description:        All value properties of the Math object should have
   the attributes [DontEnum, DontDelete, ReadOnly]

   this test checks the DontDelete attribute of Math.LN10

   Author:             christine@netscape.com
   Date:               16 september 1997
*/

var SECTION = "15.8.1.2-2";
var TITLE   = "Math.LN10";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "delete( Math.LN10 ); Math.LN10",  
	      2.302585092994046,      
	      eval("delete(Math.LN10); Math.LN10") );

new TestCase( "delete( Math.LN10 ); ",            
	      false,                 
	      eval("delete(Math.LN10)") );

test();
