/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.1.2.1-2.js
   ECMA Section:       15.1.2.1 eval(x)

   Parse x as an ECMAScript Program.  If the parse fails,
   generate a runtime error.  Evaluate the program.  If
   result is "Normal completion after value V", return
   the value V.  Else, return undefined.
   Description:
   Author:             christine@netscape.com
   Date:               16 september 1997
*/
var SECTION = "15.1.2.1-2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "eval(x)";
var BUGNUMBER = "none";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase(    SECTION,
		 "d = new Date(0); with (d) { x = getUTCMonth() +'/'+ getUTCDate() +'/'+ getUTCFullYear(); } x",
		 "0/1/1970",
		 eval( "d = new Date(0); with (d) { x = getUTCMonth() +'/'+ getUTCDate() +'/'+ getUTCFullYear(); } x" ));

test();
