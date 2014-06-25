/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          8.6.2.1-1.js
   ECMA Section:       8.6.2.1 Get (Value)
   Description:

   When the [[Get]] method of O is called with property name P, the following
   steps are taken:

   1.  If O doesn't have a property with name P, go to step 4.
   2.  Get the value of the property.
   3.  Return Result(2).
   4.  If the [[Prototype]] of O is null, return undefined.
   5.  Call the [[Get]] method of [[Prototype]] with property name P.
   6.  Return Result(5).

   This tests [[Get]] (Value).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "8.6.2.1-1";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " [[Get]] (Value)");

new TestCase( SECTION,  "var OBJ = new MyObject(true); OBJ.valueOf()",              true,           eval("var OBJ = new MyObject(true); OBJ.valueOf()") );

new TestCase( SECTION,  "var OBJ = new MyObject(Number.POSITIVE_INFINITY); OBJ.valueOf()",              Number.POSITIVE_INFINITY,           eval("var OBJ = new MyObject(Number.POSITIVE_INFINITY); OBJ.valueOf()") );

new TestCase( SECTION,  "var OBJ = new MyObject('string'); OBJ.valueOf()",              'string',           eval("var OBJ = new MyObject('string'); OBJ.valueOf()") );

test();

function MyObject( value ) {
  this.valueOf = new Function( "return this.value" );
  this.value = value;
}
