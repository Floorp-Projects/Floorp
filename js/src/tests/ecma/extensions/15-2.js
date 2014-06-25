/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15-2.js
   ECMA Section:       15 Native ECMAScript Objects

   Description:        Every built-in function and every built-in constructor
   has the Function prototype object, which is the value of
   the expression Function.prototype as the value of its
   internal [[Prototype]] property, except the Function
   prototype object itself.

   That is, the __proto__ property of builtin functions and
   constructors should be the Function.prototype object.

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15-2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Native ECMAScript Objects";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,  "Object.__proto__",   Function.prototype,   Object.__proto__ );
new TestCase( SECTION,  "Array.__proto__",    Function.prototype,   Array.__proto__ );
new TestCase( SECTION,  "String.__proto__",   Function.prototype,   String.__proto__ );
new TestCase( SECTION,  "Boolean.__proto__",  Function.prototype,   Boolean.__proto__ );
new TestCase( SECTION,  "Number.__proto__",   Function.prototype,   Number.__proto__ );
new TestCase( SECTION,  "Date.__proto__",     Function.prototype,   Date.__proto__ );
new TestCase( SECTION,  "TestCase.__proto__", Function.prototype,   TestCase.__proto__ );

new TestCase( SECTION,  "eval.__proto__",     Function.prototype,   eval.__proto__ );
new TestCase( SECTION,  "Math.pow.__proto__", Function.prototype,   Math.pow.__proto__ );
new TestCase( SECTION,  "String.prototype.indexOf.__proto__", Function.prototype,   String.prototype.indexOf.__proto__ );

test();
