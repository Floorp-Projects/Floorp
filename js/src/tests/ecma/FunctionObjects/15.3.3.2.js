/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.3.3.2.js
   ECMA Section:       15.3.3.2 Properties of the Function Constructor
   Function.length

   Description:        The length property is 1.

   Author:             christine@netscape.com
   Date:               28 october 1997

*/

var SECTION = "15.3.3.2";
var TITLE   = "Function.length";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "Function.length",  1, Function.length );

test();
