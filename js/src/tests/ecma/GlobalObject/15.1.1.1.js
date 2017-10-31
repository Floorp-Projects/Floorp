/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.1.1.1.js
   ECMA Section:       15.1.1.1 NaN

   Description:        The initial value of NaN is NaN.

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.1.1.1";
var TITLE   = "NaN";

writeHeaderToLog( SECTION + " "+ TITLE);


new TestCase( "NaN",               Number.NaN,     NaN );
new TestCase( "this.NaN",          Number.NaN,     this.NaN );
new TestCase( "typeof NaN",        "number",       typeof NaN );

test();
