/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.1.js
   ECMA Section:       15.9.5.1 Date.prototype.constructor
   Description:
   The initial value of Date.prototype.constructor is the built-in Date
   constructor.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5.1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Date.prototype.constructor";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "Date.prototype.constructor == Date",
	      true,
	      Date.prototype.constructor == Date );
test();
