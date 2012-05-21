/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          12.8-1-n.js
   ECMA Section:       12.8 The break statement
   Description:

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "12.8-1-n";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The break in statement";

writeHeaderToLog( SECTION + " "+ TITLE);

DESCRIPTION = "break";
EXPECTED = "error";

new TestCase(   SECTION,
		"break",
		"error",
		eval("break") );


test();

