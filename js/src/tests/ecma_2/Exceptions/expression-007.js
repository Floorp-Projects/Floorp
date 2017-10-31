/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          expression-007.js
   Corresponds To:     11.2.2-2-n.js
   ECMA Section:       11.2.2. The new operator
   Description:


   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "expression-007";
var TITLE   = "The new operator";

writeHeaderToLog( SECTION + " "+ TITLE);

var result = "Failed";
var exception = "No exception thrown";
var expect = "Passed";

try {
  UNDEFINED = void 0;
  result = new UNDEFINED();
} catch ( e ) {
  result = expect;
  exception = e.toString();
}

new TestCase(
  "UNDEFINED = void 0; result = new UNDEFINED()" +
  " (threw " + exception +")",
  expect,
  result );

test();

