/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.6.3.1-5.js
   ECMA Section:       15.6.3.1 Boolean.prototype
   Description:
   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var VERSION = "ECMA_2";
startTest();
var SECTION = "15.6.3.1-5";
var TITLE   = "Boolean.prototype"

  writeHeaderToLog( SECTION + " " + TITLE );

new TestCase( SECTION,  "Function.prototype == Boolean.__proto__",   true,   Function.prototype == Boolean.__proto__ );

test();
