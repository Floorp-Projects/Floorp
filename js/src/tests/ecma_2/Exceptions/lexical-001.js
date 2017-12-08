/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          lexical-001.js
   CorrespondsTo:      ecma/LexicalConventions/7.2.js
   ECMA Section:       7.2 Line Terminators
   Description:        - readability
   - separate tokens
   - may occur between any two tokens
   - cannot occur within any token, not even a string
   - affect the process of automatic semicolon insertion.

   white space characters are:
   unicode     name            formal name     string representation
   \u000A      line feed       <LF>            \n
   \u000D      carriage return <CR>            \r

   this test uses onerror to capture line numbers.  because
   we use on error, we can only have one test case per file.

   Author:             christine@netscape.com
   Date:               11 september 1997
*/
var SECTION = "lexical-001";
var TITLE   = "Line Terminators";

writeHeaderToLog( SECTION + " "+ TITLE);

var result = "Failed";
var exception = "No exception thrown";
var expect = "Passed";

try {
  result = eval("\r\n\expect");
} catch ( e ) {
  exception = e.toString();
}

new TestCase(
  "OBJECT = new Object; result = new OBJECT()" +
  " (threw " + exception +")",
  expect,
  result );

test();
