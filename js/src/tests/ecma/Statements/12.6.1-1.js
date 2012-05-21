/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          12.6.1-1.js
   ECMA Section:       The while statement
   Description:


   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "12.6.1-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The While statement";
writeHeaderToLog( SECTION + " "+ TITLE);


new TestCase( SECTION,
	      "var MYVAR = 0; while( MYVAR++ < 100) { if ( MYVAR < 100 ) break; } MYVAR ",
	      1,
	      eval("var MYVAR = 0; while( MYVAR++ < 100) { if ( MYVAR < 100 ) break; } MYVAR "));

new TestCase( SECTION,
	      "var MYVAR = 0; while( MYVAR++ < 100) { if ( MYVAR < 100 ) continue; else break; } MYVAR ",
	      100,
	      eval("var MYVAR = 0; while( MYVAR++ < 100) { if ( MYVAR < 100 ) continue; else break; } MYVAR "));

new TestCase( SECTION,
	      "function MYFUN( arg1 ) { while ( arg1++ < 100 ) { if ( arg1 < 100 ) return arg1; } }; MYFUN(1)",
	      2,
	      eval("function MYFUN( arg1 ) { while ( arg1++ < 100 ) { if ( arg1 < 100 ) return arg1; } }; MYFUN(1)"));

test();

