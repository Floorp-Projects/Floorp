/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          nesting-1.js
   Reference:          http://scopus.mcom.com/bugsplat/show_bug.cgi?id=122040
   Description:        Regression test for a nested function

   Author:             christine@netscape.com
   Date:               15 June 1998
*/

var SECTION = "function/nesting-1.js";
var VERSION = "JS_12";
startTest();
var TITLE   = "Regression test for 122040";

writeHeaderToLog( SECTION + " "+ TITLE);

function f(a) {function g(b) {return a+b;}; return g;}; f(7);

new TestCase( SECTION,
	      'function f(a) {function g(b) {return a+b;}; return g;}; typeof f(7)',
	      "function",
	      typeof f(7) );

test();

