/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.4.5.2-1.js
   ECMA Section:       Array.length
   Description:
   15.4.5.2 length
   The length property of this Array object is always numerically greater
   than the name of every property whose name is an array index.

   The length property has the attributes { DontEnum, DontDelete }.
   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.4.5.2-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Array.length";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase(   SECTION,
		"var A = new Array(); A.length",
		0,
		eval("var A = new Array(); A.length") );
new TestCase(   SECTION,
		"var A = new Array(); A[Math.pow(2,32)-2] = 'hi'; A.length",
		Math.pow(2,32)-1,
		eval("var A = new Array(); A[Math.pow(2,32)-2] = 'hi'; A.length") );
new TestCase(   SECTION,
		"var A = new Array(); A.length = 123; A.length",
		123,
		eval("var A = new Array(); A.length = 123; A.length") );
new TestCase(   SECTION,
		"var A = new Array(); A.length = 123; var PROPS = ''; for ( var p in A ) { PROPS += ( p == 'length' ? p : ''); } PROPS",
		"",
		eval("var A = new Array(); A.length = 123; var PROPS = ''; for ( var p in A ) { PROPS += ( p == 'length' ? p : ''); } PROPS") );
new TestCase(   SECTION,
		"var A = new Array(); A.length = 123; delete A.length",
		false ,
		eval("var A = new Array(); A.length = 123; delete A.length") );
new TestCase(   SECTION,
		"var A = new Array(); A.length = 123; delete A.length; A.length",
		123,
		eval("var A = new Array(); A.length = 123; delete A.length; A.length") );
test();

