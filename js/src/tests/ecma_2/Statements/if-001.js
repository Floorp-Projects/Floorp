/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 *  File Name:          if-001.js
 *  ECMA Section:
 *  Description:        The if statement
 *
 *  Verify that assignment in the if expression is evaluated correctly.
 *  Verifies the fix for bug http://scopus/bugsplat/show_bug.cgi?id=148822.
 *
 *  Author:             christine@netscape.com
 *  Date:               28 August 1998
 */
var SECTION = "for-001";
var TITLE   = "The if  statement";
var BUGNUMBER="148822";

printBugNumber(BUGNUMBER);
writeHeaderToLog( SECTION + " "+ TITLE);

var a = 0;
var b = 0;
var result = "passed";

if ( a = b ) {
  result = "failed:  a = b should return 0";
}

new TestCase(
  "if ( a = b ), where a and b are both equal to 0",
  "passed",
  result );


test();

