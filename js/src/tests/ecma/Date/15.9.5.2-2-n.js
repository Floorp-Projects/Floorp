/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.2-2.js
   ECMA Section:       15.9.5.2 Date.prototype.toString
   Description:
   This function returns a string value. The contents of the string are
   implementation dependent, but are intended to represent the Date in a
   convenient, human-readable form in the current time zone.

   The toString function is not generic; it generates a runtime error if its
   this value is not a Date object. Therefore it cannot be transferred to
   other kinds of objects for use as a method.


   This verifies that calling toString on an object that is not a string
   generates a runtime error.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5.2-2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Date.prototype.toString";

writeHeaderToLog( SECTION + " "+ TITLE);

var OBJ = new MyObject( new Date(0) );

DESCRIPTION = "var OBJ = new MyObject( new Date(0) ); OBJ.toString()";
EXPECTED = "error";

new TestCase( SECTION,
	      "var OBJ = new MyObject( new Date(0) ); OBJ.toString()",
	      "error",
	      eval("OBJ.toString()") );
test();

function MyObject( value ) {
  this.value = value;
  this.valueOf = new Function( "return this.value" );
  this.toString = Date.prototype.toString;
  return this;
}
