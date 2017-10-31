/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.8.1.8-3.js
   ECMA Section:       15.8.1.8.js
   Description:        All value properties of the Math object should have
   the attributes [DontEnum, DontDelete, ReadOnly]

   this test checks the DontDelete attribute of Math.SQRT2

   Author:             christine@netscape.com
   Date:               16 september 1997
*/
var SECTION = "15.8.1.8-3";

writeHeaderToLog( SECTION + " Math.SQRT2:  DontDelete");

new TestCase( "delete Math.SQRT2",
	      false,    
	      eval("delete Math.SQRT2") );

test();
