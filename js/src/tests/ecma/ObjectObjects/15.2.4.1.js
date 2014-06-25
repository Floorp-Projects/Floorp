/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.2.4.1.js
   ECMA Section:       15.2.4 Object.prototype.constructor

   Description:        The initial value of the Object.prototype.constructor
   is the built-in Object constructor.

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.2.4.1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Object.prototype.constructor";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION, 
	      "Object.prototype.constructor",
	      Object,
	      Object.prototype.constructor );

test();
