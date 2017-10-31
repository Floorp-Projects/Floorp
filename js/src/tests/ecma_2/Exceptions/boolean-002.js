/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          boolean-001.js
   Description:        Corresponds to ecma/Boolean/15.6.4.3-4-n.js

   15.6.4.3 Boolean.prototype.valueOf()
   Returns this boolean value.

   The valueOf function is not generic; it generates
   a runtime error if its this value is not a Boolean
   object.  Therefore it cannot be transferred to other
   kinds of objects for use as a method.

   Author:             christine@netscape.com
   Date:               09 september 1998
*/
var SECTION = "boolean-002.js";
var TITLE   = "Boolean.prototype.valueOf()";
writeHeaderToLog( SECTION +" "+ TITLE );


var exception = "No exception thrown";
var result = "Failed";

var VALUE_OF = Boolean.prototype.valueOf;

try {
  var s = new String("Not a Boolean");
  s.valueOf = VALUE_0F;
  s.valueOf();
} catch ( e ) {
  result = "Passed!";
  exception = e.toString();
}

new TestCase(
  "Assigning Boolean.prototype.valueOf to a String object "+
  "(threw " +exception +")",
  "Passed!",
  result );

test();

