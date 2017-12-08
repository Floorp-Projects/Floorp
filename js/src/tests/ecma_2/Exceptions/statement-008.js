/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          statement-008.js
   Corresponds To:     12.8-1-n.js
   ECMA Section:       12.8 The break statement
   Description:

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "statement-008";
var TITLE   = "The break in statement";

writeHeaderToLog( SECTION + " "+ TITLE);

var result = "Failed";
var exception = "No exception thrown";
var expect = "Passed";

try {
  eval("break;");
} catch ( e ) {
  result = expect;
  exception = e.toString();
}

new TestCase(
  "break outside of an iteration statement" +
  " (threw " + exception +")",
  expect,
  result );

test();

