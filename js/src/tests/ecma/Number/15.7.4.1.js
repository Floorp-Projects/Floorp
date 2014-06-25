/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.7.4.1.js
   ECMA Section:       15.7.4.1.1 Number.prototype.constructor

   Number.prototype.constructor is the built-in Number constructor.

   Description:
   Author:             christine@netscape.com
   Date:               16 september 1997
*/
var SECTION = "15.7.4.1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Number.prototype.constructor";

writeHeaderToLog( SECTION + " "+TITLE);

new TestCase(   SECTION,
		"Number.prototype.constructor",
		Number,
		Number.prototype.constructor );
test();
