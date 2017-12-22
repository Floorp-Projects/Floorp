/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
// this test originally was only seen if typed into the js shell.
// loading as a script file did not exhibit the problem.
// this test case may not exercise the problem properly.

var BUGNUMBER = 254974;
var summary = 'all var and arg properties should be JSPROP_SHARED';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

function testfunc(tokens) {
  function eek(y) {} /* remove function eek and the code will change its behavior */
  return tokens.split(/\]?(?:\[|$)/).shift();
}
bad=testfunc;
function testfunc(tokens) {
  return tokens.split(/\]?(?:\[|$)/).shift();
}
good=testfunc;

var goodvalue = good("DIV[@id=\"test\"]");
var badvalue = bad("DIV[@id=\"test\"]");

printStatus(goodvalue);
printStatus(badvalue);

expect = goodvalue;
actual = badvalue;
 
reportCompare(expect, actual, summary);
