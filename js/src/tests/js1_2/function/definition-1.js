/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          definition-1.js
   Reference:          http://scopus.mcom.com/bugsplat/show_bug.cgi?id=111284
   Description:        Regression test for declaring functions.

   Author:             christine@netscape.com
   Date:               15 June 1998
*/

var SECTION = "function/definition-1.js";
var VERSION = "JS_12";
startTest();
var TITLE   = "Regression test for 111284";

writeHeaderToLog( SECTION + " "+ TITLE);

f1 = function() { return "passed!" }

  function f2() { f3 = function() { return "passed!" }; return f3(); }

new TestCase( SECTION,
	      'f1 = function() { return "passed!" }; f1()',
	      "passed!",
	      f1() );

new TestCase( SECTION,
	      'function f2() { f3 = function { return "passed!" }; return f3() }; f2()',
	      "passed!",
	      f2() );

new TestCase( SECTION,
	      'f3()',
	      "passed!",
	      f3() );

test();

