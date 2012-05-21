/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.3.3.1-3.js
   ECMA Section:       15.3.3.1 Properties of the Function Constructor
   Function.prototype

   Description:        The initial value of Function.prototype is the built-in
   Function prototype object.

   This property shall have the attributes [DontEnum |
   DontDelete | ReadOnly]

   This test the DontDelete property of Function.prototype.

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.3.3.1-3";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Function.prototype";

writeHeaderToLog( SECTION + " "+ TITLE);


var FUN_PROTO = Function.prototype;

new TestCase(   SECTION,
		"delete Function.prototype",
		false,
		delete Function.prototype
  );

new TestCase(   SECTION,
		"delete Function.prototype; Function.prototype",
		FUN_PROTO,
		eval("delete Function.prototype; Function.prototype")
  );
test();
