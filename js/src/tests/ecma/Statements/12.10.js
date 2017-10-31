/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          12.10-1.js
   ECMA Section:       12.10 The with statement
   Description:

   Author:             christine@netscape.com
   Date:               11 september 1997
*/
var SECTION = "12.10-1";
var TITLE   = "The with statement";

writeHeaderToLog( SECTION +" "+ TITLE);

new TestCase(   "var x; with (7) x = valueOf(); typeof x;",
		"number",
		eval("var x; with(7) x = valueOf(); typeof x;") );

test();
