/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          12.6.2-9-n.js
   ECMA Section:       12.6.2 The for Statement

   1. first expression is not present.
   2. second expression is not present
   3. third expression is not present


   Author:             christine@netscape.com
   Date:               15 september 1997
*/


var SECTION = "12.6.2-9-n";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The for statement";

writeHeaderToLog( SECTION + " "+ TITLE);

DESCRIPTION = "for (i)";
EXPECTED = "error";

new TestCase( SECTION,
	      "for (i)",
	      "error",
	      eval("for (i) { }") );

/*
  for (i) {
  }

*/
test();

