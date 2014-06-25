/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.5.5.1
   ECMA Section:       String.length
   Description:

   The number of characters in the String value represented by this String
   object.

   Once a String object is created, this property is unchanging. It has the
   attributes { DontEnum, DontDelete, ReadOnly }.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.5.5.1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "String.length";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase(   SECTION,
		"var s = new String(); s.length",
		0,
		eval("var s = new String(); s.length") );

new TestCase(   SECTION,
		"var s = new String(); s.length = 10; s.length",
		0,
		eval("var s = new String(); s.length = 10; s.length") );

new TestCase(   SECTION,
		"var s = new String(); var props = ''; for ( var p in s ) {  props += p; };  props",
		"",
		eval("var s = new String(); var props = ''; for ( var p in s ) {  props += p; };  props") );

new TestCase(   SECTION,
		"var s = new String(); delete s.length",
		false,
		eval("var s = new String(); delete s.length") );

new TestCase(   SECTION,
		"var s = new String('hello'); delete s.length; s.length",
		5,
		eval("var s = new String('hello'); delete s.length; s.length") );

test();
