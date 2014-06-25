/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.1-2-n.js
   ECMA Section:       The global object
   Description:

   The global object does not have a [[Call]] property; it is not possible
   to invoke the global object as a function.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.1-2-n";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The Global Object";

writeHeaderToLog( SECTION + " "+ TITLE);

DESCRIPTION = "var MY_GLOBAL = this()";
EXPECTED = "error";

new TestCase(   SECTION,
		"var MY_GLOBAL = this()",
		"error",
		eval("var MY_GLOBAL = this()") );
test();
