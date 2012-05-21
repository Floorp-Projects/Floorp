/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.6.3.1-4.js
   ECMA Section:       15.6.3.1 Properties of the Boolean Prototype Object

   Description:        The initial value of Boolean.prototype is the built-in
   Boolean prototype object (15.6.4).

   The property shall have the attributes [DontEnum,
   DontDelete, ReadOnly ].

   This tests the ReadOnly property of Boolean.prototype

   Author:             christine@netscape.com
   Date:               30 september 1997

*/
var SECTION = "15.6.3.1-4";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Boolean.prototype"
  writeHeaderToLog( SECTION + TITLE );

var BOOL_PROTO = Boolean.prototype;

new TestCase( SECTION,
	      "var BOOL_PROTO = Boolean.prototype; Boolean.prototype=null; Boolean.prototype == BOOL_PROTO",
	      true,
	      eval("var BOOL_PROTO = Boolean.prototype; Boolean.prototype=null; Boolean.prototype == BOOL_PROTO") );

new TestCase( SECTION,
	      "var BOOL_PROTO = Boolean.prototype; Boolean.prototype=null; Boolean.prototype == null",
	      false,
	      eval("var BOOL_PROTO = Boolean.prototype; Boolean.prototype=null; Boolean.prototype == null") );

test();
