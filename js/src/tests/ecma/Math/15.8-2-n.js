/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.8-2.js
   ECMA Section:       15.8 The Math Object

   Description:

   The Math object is merely a single object that has some named properties,
   some of which are functions.

   The value of the internal [[Prototype]] property of the Math object is the
   Object prototype object (15.2.3.1).

   The Math object does not have a [[Construct]] property; it is not possible
   to use the Math object as a constructor with the new operator.

   The Math object does not have a [[Call]] property; it is not possible to
   invoke the Math object as a function.

   Recall that, in this specification, the phrase "the number value for x" has
   a technical meaning defined in section 8.5.

   Author:             christine@netscape.com
   Date:               12 november 1997

*/

var SECTION = "15.8-2-n";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The Math Object";

writeHeaderToLog( SECTION + " "+ TITLE);

DESCRIPTION = "MYMATH = new Math()";
EXPECTED = "error";

new TestCase( SECTION,
	      "MYMATH = new Math()",
	      "error",
	      eval("MYMATH = new Math()") );

test();
