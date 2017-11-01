/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.7.4.js
   ECMA Section:       15.7.4

   Description:

   The Number prototype object is itself a Number object (its [[Class]] is
   "Number") whose value is +0.

   The value of the internal [[Prototype]] property of the Number prototype
   object is the Object prototype object (15.2.3.1).

   In following descriptions of functions that are properties of the Number
   prototype object, the phrase "this Number object" refers to the object
   that is the this value for the invocation of the function; it is an error
   if this does not refer to an object for which the value of the internal
   [[Class]] property is "Number". Also, the phrase "this number value" refers
   to the number value represented by this Number object, that is, the value
   of the internal [[Value]] property of this Number object.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "15.7.4";
var TITLE   = "Properties of the Number Prototype Object";

writeHeaderToLog( SECTION + " "+TITLE);

new TestCase( "Number.prototype.toString=Object.prototype.toString;Number.prototype.toString()",
	      "[object Number]",
	      eval("Number.prototype.toString=Object.prototype.toString;Number.prototype.toString()") );

new TestCase( "typeof Number.prototype",  
	      "object",
	      typeof Number.prototype );

new TestCase( "Number.prototype.valueOf()", 
	      0,
	      Number.prototype.valueOf() );

//    The __proto__ property cannot be used in ECMA_1 tests.
//    new TestCase( "Number.prototype.__proto__",                        Object.prototype,   Number.prototype.__proto__ );
//    new TestCase( "Number.prototype.__proto__ == Object.prototype",    true,       Number.prototype.__proto__ == Object.prototype );

test();
