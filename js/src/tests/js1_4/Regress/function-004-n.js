/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 *  File Name:          function-004.js
 *  Description:
 *
 *  http://scopus.mcom.com/bugsplat/show_bug.cgi?id=310502
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 */
var SECTION = "funtion-004-n.js";
var TITLE   = "Regression test case for 310502";
var BUGNUMBER="310502";

printBugNumber(BUGNUMBER);

writeHeaderToLog( SECTION + " "+ TITLE);

var o  = {};
o.call = Function.prototype.call;

DESCRIPTION = "var o = {}; o.call = Function.prototype.call; o.call()";

new TestCase(
  "var o = {}; o.call = Function.prototype.call; o.call()",
  "error",
  o.call() );

test();
