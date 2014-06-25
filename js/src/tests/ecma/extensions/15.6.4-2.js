/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.6.4-2.js
   ECMA Section:       15.6.4 Properties of the Boolean Prototype Object

   Description:
   The Boolean prototype object is itself a Boolean object (its [[Class]] is
   "Boolean") whose value is false.

   The value of the internal [[Prototype]] property of the Boolean prototype object
   is the Object prototype object (15.2.3.1).

   Author:             christine@netscape.com
   Date:               30 september 1997

*/


var VERSION = "ECMA_2"
  startTest();
var SECTION = "15.6.4-2";

writeHeaderToLog( SECTION + " Properties of the Boolean Prototype Object");

new TestCase( SECTION, "Boolean.prototype.__proto__",               Object.prototype,       Boolean.prototype.__proto__ );

test();
