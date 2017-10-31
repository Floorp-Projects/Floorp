/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          statement-005.js
   Corresponds To:     12.6.3-8-n.js
   ECMA Section:       12.6.3 The for...in Statement
   Description:
   Author:             christine@netscape.com
   Date:               11 september 1997
*/
var SECTION = "statement-005";
var TITLE   = "The for..in statement";

writeHeaderToLog( SECTION + " "+ TITLE);

var result = "Failed";
var exception = "No exception thrown";
var expect = "Passed";

try {
  var o = new MyObject();
  result = 0;

  eval("for (1 in o) {\n"
       + "result += this[p];"
       + "}\n");
} catch ( e ) {
  result = expect;
  exception = e.toString();
}

new TestCase(
  "bad left-hand side expression" +
  " (threw " + exception +")",
  expect,
  result );

test();

function MyObject() {
  this.value = 2;
  this[0] = 4;
  return this;
}
