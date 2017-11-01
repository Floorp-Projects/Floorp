/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          expression-004.js
   Corresponds To:     11.2.1-4-n.js
   ECMA Section:       11.2.1 Property Accessors
   Description:

   Author:             christine@netscape.com
   Date:               09 september 1998
*/
var SECTION = "expression-004";
var TITLE   = "Property Accessors";
writeHeaderToLog( SECTION + " "+TITLE );

var OBJECT = new Property( "null", null, "null", 0 );

var result    = "Failed";
var exception = "No exception thrown";
var expect    = "Passed";

try {
  result = OBJECT.value.toString();
} catch ( e ) {
  result = expect;
  exception = e.toString();
}

new TestCase(
  "Get the toString value of an object whose value is null "+
  "(threw " + exception +")",
  expect,
  result );

test();

function Property( object, value, string, number ) {
  this.object = object;
  this.string = String(value);
  this.number = Number(value);
  this.value = value;
}
