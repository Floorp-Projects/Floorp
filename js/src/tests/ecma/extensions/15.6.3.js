/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.6.3.js
   ECMA Section:       15.6.3 Properties of the Boolean Constructor

   Description:        The value of the internal prototype property is
   the Function prototype object.

   It has the internal [[Call]] and [[Construct]]
   properties, and the length property.

   Author:             christine@netscape.com
   Date:               june 27, 1997

*/
var SECTION = "15.6.3";
var VERSION = "ECMA_2";
startTest();
var TITLE   = "Properties of the Boolean Constructor"
  writeHeaderToLog( SECTION + TITLE );


new TestCase( SECTION,  "Boolean.__proto__ == Function.prototype",  true,   Boolean.__proto__ == Function.prototype );
new TestCase( SECTION,  "Boolean.length",          1,                   Boolean.length );

test();
