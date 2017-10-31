/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.6.4.js
   ECMA Section:       Properties of the Boolean Prototype Object
   Description:
   The Boolean prototype object is itself a Boolean object (its [[Class]] is "
   Boolean") whose value is false.

   The value of the internal [[Prototype]] property of the Boolean prototype
   object is the Object prototype object (15.2.3.1).

   In following descriptions of functions that are properties of the Boolean
   prototype object, the phrase "this Boolean object" refers to the object that
   is the this value for the invocation of the function; it is an error if
   this does not refer to an object for which the value of the internal
   [[Class]] property is "Boolean". Also, the phrase "this boolean value"
   refers to the boolean value represented by this Boolean object, that is,
   the value of the internal [[Value]] property of this Boolean object.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "15.6.4";
var TITLE   = "Properties of the Boolean Prototype Object";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "Boolean.prototype == false",
	      true,
	      Boolean.prototype == false );

new TestCase( "Boolean.prototype.toString = Object.prototype.toString; Boolean.prototype.toString()",
	      "[object Boolean]",
	      eval("Boolean.prototype.toString = Object.prototype.toString; Boolean.prototype.toString()") );

test();
