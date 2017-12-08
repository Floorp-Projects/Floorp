/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          lexical-051.js
   Corresponds to:     7.8.2-3-n.js
   ECMA Section:       7.8.2 Examples of Automatic Semicolon Insertion
   Description:        compare some specific examples of the automatic
   insertion rules in the EMCA specification.
   Author:             christine@netscape.com
   Date:               15 september 1997
*/

var SECTION = "lexical-051";
var TITLE   = "Examples of Automatic Semicolon Insertion";

writeHeaderToLog( SECTION + " "+ TITLE);

var result = "Failed";
var exception = "No exception thrown";
var expect = "Passed";

try {
  eval("for (a; b\n) result += \": got to inner loop\";")
    } catch ( e ) {
  result = expect;
  exception = e.toString();
}

new TestCase(
  "for (a; b\n)" +
  " (threw " + exception +")",
  expect,
  result );

test();



