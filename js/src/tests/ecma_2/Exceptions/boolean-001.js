/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          boolean-001.js
   Description:        Corresponds to ecma/Boolean/15.6.4.2-4-n.js

   The toString function is not generic; it generates
   a runtime error if its this value is not a Boolean
   object.  Therefore it cannot be transferred to other
   kinds of objects for use as a method.

   Author:             christine@netscape.com
   Date:               june 27, 1997
*/
var SECTION = "boolean-001.js";
var TITLE   = "Boolean.prototype.toString()";
writeHeaderToLog( SECTION +" "+ TITLE );

var exception = "No exception thrown";
var result = "Failed";

var TO_STRING = Boolean.prototype.toString;

try {
  var s = new String("Not a Boolean");
  s.toString = TO_STRING;
  s.toString();
} catch ( e ) {
  result = "Passed!";
  exception = e.toString();
}

new TestCase(
  "Assigning Boolean.prototype.toString to a String object "+
  "(threw " +exception +")",
  "Passed!",
  result );

test();

