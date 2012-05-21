/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          8.1.js
   ECMA Section:       The undefined type
   Description:

   The Undefined type has exactly one value, called undefined. Any variable
   that has not been assigned a value is of type Undefined.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "8.1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The undefined type";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "var x; typeof x",
	      "undefined",
	      eval("var x; typeof x") );

new TestCase( SECTION,
	      "var x; typeof x == 'undefined",
	      true,
	      eval("var x; typeof x == 'undefined'") );

new TestCase( SECTION,
	      "var x; x == void 0",
	      true,
	      eval("var x; x == void 0") );
test();

