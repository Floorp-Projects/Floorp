/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.3.5-1.js
   ECMA Section:       15.3.5 Properties of Function Instances
   new Function(p1, p2, ..., pn, body )

   Description:

   15.3.5.1 length

   The value of the length property is usually an integer that indicates
   the "typical" number of arguments expected by the function. However,
   the language permits the function to be invoked with some other number
   of arguments. The behavior of a function when invoked on a number of
   arguments other than the number specified by its length property depends
   on the function.

   15.3.5.2 prototype
   The value of the prototype property is used to initialize the internal [[
   Prototype]] property of a newly created object before the Function object
   is invoked as a constructor for that newly created object.

   15.3.5.3 arguments

   The value of the arguments property is normally null if there is no
   outstanding invocation of the function in progress (that is, the function has been called
   but has not yet returned). When a non-internal Function object (15.3.2.1) is invoked, its
   arguments property is "dynamically bound" to a newly created object that contains the
   arguments on which it was invoked (see 10.1.6 and 10.1.8). Note that the use of this
   property is discouraged; it is provided principally for compatibility with existing old code.

   Author:             christine@netscape.com
   Date:               28 october 1997

*/

var SECTION = "15.3.5-2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Properties of Function Instances";

writeHeaderToLog( SECTION + " "+TITLE);

var MyObject = new Function( 'a', 'b', 'c', 'this.a = a; this.b = b; this.c = c; this.value = a+b+c; this.valueOf = new Function( "return this.value" )' );

new TestCase( SECTION, "MyObject.length",                       3,          MyObject.length );
new TestCase( SECTION, "typeof MyObject.prototype",             "object",   typeof MyObject.prototype );
new TestCase( SECTION, "typeof MyObject.prototype.constructor", "function", typeof MyObject.prototype.constructor );
new TestCase( SECTION, "MyObject.arguments",                     null,       MyObject.arguments );

test();
