/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          expression-014.js
   Corresponds To:     ecma/Expressions/11.2.2-9-n.js
   ECMA Section:       11.2.2. The new operator
   Description:

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "expression-014.js";
var VERSION = "ECMA_1";
var TITLE   = "The new operator";
var BUGNUMBER= "327765";

startTest();
writeHeaderToLog( SECTION + " "+ TITLE);

var BOOLEAN = new Boolean();


var result = "Failed";
var exception = "No exception thrown";
var expect = "Passed";

try {
  result = new BOOLEAN();
} catch ( e ) {
  result = expect;
  exception = e.toString();
}

new TestCase(
  SECTION,
  "BOOLEAN = new Boolean(); result = new BOOLEAN()" +
  " (threw " + exception +")",
  expect,
  result );

test();

