/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.6.3.1.js
   ECMA Section:       15.6.3.1 Boolean.prototype

   Description:        The initial valu eof Boolean.prototype is the built-in
   Boolean prototype object (15.6.4).

   The property shall have the attributes [DontEnum,
   DontDelete, ReadOnly ].

   It has the internal [[Call]] and [[Construct]]
   properties (not tested), and the length property.

   Author:             christine@netscape.com
   Date:               june 27, 1997

*/

var SECTION = "15.6.3.1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Boolean.prototype";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,  "Boolean.prototype.valueOf()",       false,   Boolean.prototype.valueOf() );
new TestCase( SECTION,  "Boolean.length",          1,       Boolean.length );

test();
