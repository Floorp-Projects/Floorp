/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.6.4.1.js
   ECMA Section:       15.6.4.1 Boolean.prototype.constructor

   Description:        The initial value of Boolean.prototype.constructor
   is the built-in Boolean constructor.

   Author:             christine@netscape.com
   Date:               30 september 1997

*/
var SECTION = "15.6.4.1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Boolean.prototype.constructor"
  writeHeaderToLog( SECTION + TITLE );

new TestCase( SECTION,
	      "( Boolean.prototype.constructor == Boolean )",
	      true ,
	      (Boolean.prototype.constructor == Boolean) );
test();
