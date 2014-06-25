/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.3.4.js
   ECMA Section:       15.3.4  Properties of the Function Prototype Object

   Description:        The Function prototype object is itself a Function
   object ( its [[Class]] is "Function") that, when
   invoked, accepts any arguments and returns undefined.

   The value of the internal [[Prototype]] property
   object is the Object prototype object.

   It is a function with an "empty body"; if it is
   invoked, it merely returns undefined.

   The Function prototype object does not have a valueOf
   property of its own; however it inherits the valueOf
   property from the Object prototype Object.

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.3.4";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Properties of the Function Prototype Object";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase(   SECTION,
		"var myfunc = Function.prototype; myfunc.toString = Object.prototype.toString; myfunc.toString()",
		"[object Function]",
		eval("var myfunc = Function.prototype; myfunc.toString = Object.prototype.toString; myfunc.toString()"));


//  new TestCase( SECTION,  "Function.prototype.__proto__",     Object.prototype,           Function.prototype.__proto__ );
new TestCase( SECTION,  "Function.prototype.valueOf",       Object.prototype.valueOf,   Function.prototype.valueOf );
new TestCase( SECTION,  "Function.prototype()",             (void 0),                   Function.prototype() );
new TestCase( SECTION,  "Function.prototype(1,true,false,'string', new Date(),null)",  (void 0), Function.prototype(1,true,false,'string', new Date(),null) );

test();
