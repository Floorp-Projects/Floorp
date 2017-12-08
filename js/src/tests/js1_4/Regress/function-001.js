/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
 *  File Name:          function-001.js
 *  Description:
 *
 * http://scopus.mcom.com/bugsplat/show_bug.cgi?id=325843
 * js> function f(a){var a,b;}
 *
 * causes an an assert on a null 'sprop' in the 'Variables' function in
 * jsparse.c This will crash non-debug build.
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 */
var SECTION = "function-001.js";
var TITLE   = "Regression test case for 325843";
var BUGNUMBER="3258435";
printBugNumber(BUGNUMBER);

writeHeaderToLog( SECTION + " "+ TITLE);

eval("function f1 (a){ var a,b; }");

function f2( a ) { var a, b; };

new TestCase(
  "eval(\"function f1 (a){ var a,b; }\"); "+
  "function f2( a ) { var a, b; }; typeof f1",
  "function",
  typeof f1 );

// force a function decompilation

new TestCase(
  "typeof f1.toString()",
  "string",
  typeof f1.toString() );

new TestCase(
  "typeof f2",
  "function",
  typeof f2 );

// force a function decompilation

new TestCase(
  "typeof f2.toString()",
  "string",
  typeof f2.toString() );

test();

