/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.5.4.2.js
   ECMA Section:       15.5.4.2 String.prototype.toString

   Description:
   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.5.4.2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "String.prototype.tostring";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION, "String.prototype.toString.__proto__",  Function.prototype, String.prototype.toString.__proto__ );

test();
