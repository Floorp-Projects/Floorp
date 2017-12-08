/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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
var TITLE   = "Infinity";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "Infinity",               Number.POSITIVE_INFINITY,      Infinity );
new TestCase( "this.Infinity",          Number.POSITIVE_INFINITY,      this.Infinity );
new TestCase( "typeof Infinity",        "number",                      typeof Infinity );

test();
