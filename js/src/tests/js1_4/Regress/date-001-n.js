/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 *  File Name:          date-001-n.js
 *  Description:
 *
 *  http://scopus.mcom.com/bugsplat/show_bug.cgi?id=299903
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 */
var SECTION = "date-001-n.js";
var VERSION = "JS1_4";
var TITLE   = "Regression test case for 299903";
var BUGNUMBER="299903";

startTest();

writeHeaderToLog( SECTION + " "+ TITLE);

function MyDate() {
  this.foo = "bar";
}
MyDate.prototype = new Date();

DESCRIPTION =
  "function MyDate() { this.foo = \"bar\"; }; MyDate.prototype = new Date(); new MyDate().toString()";
EXPECTED = "error";

new TestCase(
  SECTION,
  "function MyDate() { this.foo = \"bar\"; }; "+
  "MyDate.prototype = new Date(); " +
  "new MyDate().toString()",
  "error",
  new MyDate().toString() );

test();
