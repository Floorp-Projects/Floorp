/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.js
   ECMA Section:       15 Native ECMAScript Objects
   Description:        Every built-in prototype object has the Object prototype
   object, which is the value of the expression
   Object.prototype (15.2.3.1) as the value of its internal
   [[Prototype]] property, except the Object prototype
   object itself.

   Every native object associated with a program-created
   function also has the Object prototype object as the
   value of its internal [[Prototype]] property.

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Native ECMAScript Objects";

writeHeaderToLog( SECTION + " "+ TITLE);
/*
  new TestCase( SECTION,  "Function.prototype.__proto__", Object.prototype,   Function.prototype.__proto__ );
  new TestCase( SECTION,  "Array.prototype.__proto__",    Object.prototype,   Array.prototype.__proto__ );
  new TestCase( SECTION,  "String.prototype.__proto__",   Object.prototype,   String.prototype.__proto__ );
  new TestCase( SECTION,  "Boolean.prototype.__proto__",  Object.prototype,   Boolean.prototype.__proto__ );
  new TestCase( SECTION,  "Number.prototype.__proto__",   Object.prototype,   Number.prototype.__proto__ );
//    new TestCase( SECTION,  "Math.prototype.__proto__",     Object.prototype,   Math.prototype.__proto__ );
new TestCase( SECTION,  "Date.prototype.__proto__",     Object.prototype,   Date.prototype.__proto__ );
new TestCase( SECTION,  "TestCase.prototype.__proto__", Object.prototype,   TestCase.prototype.__proto__ );

new TestCase( SECTION,  "MyObject.prototype.__proto__", Object.prototype,   MyObject.prototype.__proto__ );
*/
new TestCase( SECTION,  "Function.prototype.__proto__ == Object.prototype", true,   Function.prototype.__proto__ == Object.prototype );
new TestCase( SECTION,  "Array.prototype.__proto__ == Object.prototype",    true,   Array.prototype.__proto__ == Object.prototype );
new TestCase( SECTION,  "String.prototype.__proto__ == Object.prototype",   true,   String.prototype.__proto__ == Object.prototype );
new TestCase( SECTION,  "Boolean.prototype.__proto__ == Object.prototype",  true,   Boolean.prototype.__proto__ == Object.prototype );
new TestCase( SECTION,  "Number.prototype.__proto__ == Object.prototype",   true,   Number.prototype.__proto__ == Object.prototype );
//    new TestCase( SECTION,  "Math.prototype.__proto__ == Object.prototype",     true,   Math.prototype.__proto__ == Object.prototype );
new TestCase( SECTION,  "Date.prototype.__proto__ == Object.prototype",     true,   Date.prototype.__proto__ == Object.prototype );
new TestCase( SECTION,  "TestCase.prototype.__proto__ == Object.prototype", true,   TestCase.prototype.__proto__ == Object.prototype );

new TestCase( SECTION,  "MyObject.prototype.__proto__ == Object.prototype", true,   MyObject.prototype.__proto__ == Object.prototype );


test();


function MyObject( value ) {
  this.value = value;
  this.valueOf = new Function( "return this.value" );
}
