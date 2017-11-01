/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          string-002.js
   Corresponds To:     15.5.4.3-3-n.js
   ECMA Section:       15.5.4.3 String.prototype.valueOf()

   Description:        Returns this string value.

   The valueOf function is not generic; it generates a
   runtime error if its this value is not a String object.
   Therefore it connot be transferred to the other kinds of
   objects for use as a method.

   Author:             christine@netscape.com
   Date:               1 october 1997
*/
var SECTION = "string-002";
var TITLE   = "String.prototype.valueOf";

writeHeaderToLog( SECTION + " "+ TITLE);

var result = "Failed";
var exception = "No exception thrown";
var expect = "Passed";

try {
  var OBJECT =new Object();
  OBJECT.valueOf = String.prototype.valueOf;
  result = OBJECT.valueOf();
} catch ( e ) {
  result = expect;
  exception = e.toString();
}

new TestCase(
  "OBJECT = new Object; OBJECT.valueOf = String.prototype.valueOf;"+
  "result = OBJECT.valueOf();" +
  " (threw " + exception +")",
  expect,
  result );

test();


