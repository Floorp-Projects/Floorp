/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          switch-001.js
   Section:
   Description:

   http://scopus.mcom.com/bugsplat/show_bug.cgi?id=315767

   Verify that switches do not use strict equality in
   versions of JavaScript < 1.4.  It's now been decided that
   we won't put in version switches, so all switches will
   be ECMA.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "switch-001";
var TITLE   = "switch-001";
var BUGNUMBER="315767";

printBugNumber(BUGNUMBER);
writeHeaderToLog( SECTION + " "+ TITLE);

result = "fail:  did not enter switch";

switch (true) {
case 1:
  result = "fail:  version 130 should force strict equality";
  break;
case true:
  result = "pass";
  break;
default:
  result = "fail: evaluated default statement";
}

new TestCase(
  "switch / case should use strict equality in version of JS < 1.4",
  "pass",
  result );

test();

