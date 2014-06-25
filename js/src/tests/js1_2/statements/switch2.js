/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   Filename:     switch2.js
   Description:  'Tests the switch statement'

   http://scopus.mcom.com/bugsplat/show_bug.cgi?id=323696

   Author:       Norris Boyd
   Date:         July 31, 1998
*/

var SECTION = 'As described in Netscape doc "Whats new in JavaScript 1.2"';
var VERSION = 'no version';
var TITLE   = 'statements: switch';
var BUGNUMBER="323626";

startTest();
writeHeaderToLog("Executing script: switch2.js");
writeHeaderToLog( SECTION + " "+ TITLE);

// test defaults not at the end; regression test for a bug that
// nearly made it into 4.06
function f0(i) {
  switch(i) {
  default:
  case "a":
  case "b":
    return "ab*"
      case "c":
    return "c";
  case "d":
    return "d";
  }
  return "";
}
new TestCase(SECTION, 'switch statement',
	     f0("a"), "ab*");

new TestCase(SECTION, 'switch statement',
	     f0("b"), "ab*");

new TestCase(SECTION, 'switch statement',
	     f0("*"), "ab*");

new TestCase(SECTION, 'switch statement',
	     f0("c"), "c");

new TestCase(SECTION, 'switch statement',
	     f0("d"), "d");

function f1(i) {
  switch(i) {
  case "a":
  case "b":
  default:
    return "ab*"
      case "c":
    return "c";
  case "d":
    return "d";
  }
  return "";
}

new TestCase(SECTION, 'switch statement',
	     f1("a"), "ab*");

new TestCase(SECTION, 'switch statement',
	     f1("b"), "ab*");

new TestCase(SECTION, 'switch statement',
	     f1("*"), "ab*");

new TestCase(SECTION, 'switch statement',
	     f1("c"), "c");

new TestCase(SECTION, 'switch statement',
	     f1("d"), "d");

// Switch on integer; will use TABLESWITCH opcode in C engine
function f2(i) {
  switch (i) {
  case 0:
  case 1:
    return 1;
  case 2:
    return 2;
  }
  // with no default, control will fall through
  return 3;
}

new TestCase(SECTION, 'switch statement',
	     f2(0), 1);

new TestCase(SECTION, 'switch statement',
	     f2(1), 1);

new TestCase(SECTION, 'switch statement',
	     f2(2), 2);

new TestCase(SECTION, 'switch statement',
	     f2(3), 3);

// empty switch: make sure expression is evaluated
var se = 0;
switch (se = 1) {
}
new TestCase(SECTION, 'switch statement',
	     se, 1);

// only default
se = 0;
switch (se) {
default:
  se = 1;
}
new TestCase(SECTION, 'switch statement',
	     se, 1);

// in loop, break should only break out of switch
se = 0;
for (var i=0; i < 2; i++) {
  switch (i) {
  case 0:
  case 1:
    break;
  }
  se = 1;
}
new TestCase(SECTION, 'switch statement',
	     se, 1);

// test "fall through"
se = 0;
i = 0;
switch (i) {
case 0:
  se++;
  /* fall through */
case 1:
  se++;
  break;
}
new TestCase(SECTION, 'switch statement',
	     se, 2);
print("hi");

test();

// Needed: tests for evaluation time of case expressions.
// This issue was under debate at ECMA, so postponing for now.

