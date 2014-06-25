/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          function-002.js
   Section:
   Description:

   http://scopus.mcom.com/bugsplat/show_bug.cgi?id=249579

   function definitions in conditional statements should be allowed.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "function-002";
var VERSION = "JS1_3";
var TITLE   = "Regression test for 249579";
var BUGNUMBER="249579";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase(
  SECTION,
  "0?function(){}:0",
  0,
  0?function(){}:0 );


bar = true;
foo = bar ? function () { return true; } : function() { return false; };

new TestCase(
  SECTION,
  "bar = true; foo = bar ? function () { return true; } : function() { return false; }; foo()",
  true,
  foo() );


test();

