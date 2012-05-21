/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.1.1.2.js
   ECMA Section:       15.1.1.2 Infinity

   Description:        The initial value of Infinity is +Infinity.

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.1.1.2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Infinity";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION, "Infinity",               Number.POSITIVE_INFINITY,      Infinity );
new TestCase( SECTION, "this.Infinity",          Number.POSITIVE_INFINITY,      this.Infinity );
new TestCase( SECTION, "typeof Infinity",        "number",                      typeof Infinity );

test();
