/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.2.4.js
   ECMA Section:       15.2.4 Properties of the Object prototype object

   Description:        The value of the internal [[Prototype]] property of
   the Object prototype object is null

   Author:             christine@netscape.com
   Date:               28 october 1997

*/

var SECTION = "15.2.4";
var VERSION = "ECMA_2";
startTest();
var TITLE   = "Properties of the Object.prototype object";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION, 
	      "Object.prototype.__proto__",
	      null,
	      Object.prototype.__proto__ );

test();

