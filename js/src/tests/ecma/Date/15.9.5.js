/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.5.js
   ECMA Section:       15.9.5 Properties of the Date prototype object
   Description:

   The Date prototype object is itself a Date object (its [[Class]] is
   "Date") whose value is NaN.

   The value of the internal [[Prototype]] property of the Date prototype
   object is the Object prototype object (15.2.3.1).

   In following descriptions of functions that are properties of the Date
   prototype object, the phrase "this Date object" refers to the object that
   is the this value for the invocation of the function; it is an error if
   this does not refer to an object for which the value of the internal
   [[Class]] property is "Date". Also, the phrase "this time value" refers
   to the number value for the time represented by this Date object, that is,
   the value of the internal [[Value]] property of this Date object.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.9.5";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Properties of the Date Prototype Object";

writeHeaderToLog( SECTION + " "+ TITLE);


Date.prototype.getClass = Object.prototype.toString;

new TestCase( SECTION,
	      "Date.prototype.getClass",
	      "[object Object]",
	      Date.prototype.getClass() );
new TestCase( SECTION,
	      "Date.prototype.valueOf()",
	      "TypeError",
	      (function() { try { Date.prototype.valueOf() } catch (e) { return e.constructor.name; } })());
new TestCase( SECTION,
          "Date.prototype.__proto__ == Object.prototype",
          true,
          Date.prototype.__proto__ == Object.prototype );
test();
