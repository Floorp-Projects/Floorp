/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.1-1-n.js
   ECMA Section:       The global object
   Description:

   The global object does not have a [[Construct]] property; it is not
   possible to use the global object as a constructor with the new operator.


   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.1-1-n";
var TITLE   = "The Global Object";

writeHeaderToLog( SECTION + " "+ TITLE);

DESCRIPTION = "var MY_GLOBAL = new this()";

new TestCase(   "var MY_GLOBAL = new this()",
		"error",
		eval("var MY_GLOBAL = new this()") );

test();

