/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          7.7.2.js
   ECMA Section:       7.7.2 Boolean Literals

   Description:        BooleanLiteral::
   true
   false

   The value of the Boolean literal true is a value of the
   Boolean type, namely true.

   The value of the Boolean literal false is a value of the
   Boolean type, namely false.

   Author:             christine@netscape.com
   Date:               16 september 1997
*/

var SECTION = "7.7.2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Boolean Literals";

writeHeaderToLog( SECTION + " "+ TITLE);

// StringLiteral:: "" and ''

new TestCase( SECTION, "true",     Boolean(true),     true );
new TestCase( SECTION, "false",    Boolean(false),    false );

test();
