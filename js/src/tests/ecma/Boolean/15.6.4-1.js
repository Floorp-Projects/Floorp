/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.6.4-1.js
   ECMA Section:       15.6.4 Properties of the Boolean Prototype Object

   Description:
   The Boolean prototype object is itself a Boolean object (its [[Class]] is
   "Boolean") whose value is false.

   The value of the internal [[Prototype]] property of the Boolean prototype object
   is the Object prototype object (15.2.3.1).

   Author:             christine@netscape.com
   Date:               30 september 1997

*/


var SECTION = "15.6.4-1";

writeHeaderToLog( SECTION + " Properties of the Boolean Prototype Object");

new TestCase( "typeof Boolean.prototype == typeof( new Boolean )", true,          typeof Boolean.prototype == typeof( new Boolean ) );
new TestCase( "typeof( Boolean.prototype )",              "object",               typeof(Boolean.prototype) );
new TestCase( 
	      "Boolean.prototype.toString = Object.prototype.toString; Boolean.prototype.toString()",
	      "[object Boolean]",
	      eval("Boolean.prototype.toString = Object.prototype.toString; Boolean.prototype.toString()") );
new TestCase( "Boolean.prototype.valueOf()",               false,                  Boolean.prototype.valueOf() );

test();
