/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.3.4.1.js
   ECMA Section:       15.3.4.1  Function.prototype.constructor

   Description:        The initial value of Function.prototype.constructor
   is the built-in Function constructor.
   Author:             christine@netscape.com
   Date:               28 october 1997

*/

var SECTION = "15.3.4.1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Function.prototype.constructor";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION, "Function.prototype.constructor",   Function,   Function.prototype.constructor );

test();
