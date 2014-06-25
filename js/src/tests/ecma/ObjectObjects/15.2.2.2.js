/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.2.2.2.js
   ECMA Section:       15.2.2.2 new Object()
   Description:

   When the Object constructor is called with no argument, the following
   step is taken:

   1.  Create a new native ECMAScript object.
   The [[Prototype]] property of the newly constructed object is set to
   the Object prototype object.

   The [[Class]] property of the newly constructed object is set
   to "Object".

   The newly constructed object has no [[Value]] property.

   Return the newly created native object.

   Author:             christine@netscape.com
   Date:               7 october 1997
*/
var SECTION = "15.2.2.2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "new Object()";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION, "typeof new Object()",   "object",       typeof new Object() );
new TestCase( SECTION, "Object.prototype.toString()",   "[object Object]",  Object.prototype.toString() );
new TestCase( SECTION, "(new Object()).toString()",  "[object Object]",   (new Object()).toString() );

test();
