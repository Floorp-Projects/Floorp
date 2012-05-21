/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          10.2.1.js
   ECMA Section:       10.2.1 Global Code
   Description:

   The scope chain is created and initialized to contain the global object and
   no others.

   Variable instantiation is performed using the global object as the variable
   object and using empty property attributes.

   The this value is the global object.
   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "10.2.1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Global Code";

writeHeaderToLog( SECTION + " "+ TITLE);

var THIS = this;

new TestCase( SECTION,
	      "this +''",
	      GLOBAL,
	      THIS + "" );

var GLOBAL_PROPERTIES = new Array();
var i = 0;

for ( p in this ) {
  GLOBAL_PROPERTIES[i++] = p;
}

for ( i = 0; i < GLOBAL_PROPERTIES.length; i++ ) {
  new TestCase( SECTION,
		GLOBAL_PROPERTIES[i] +" == void 0",
		false,
		eval("GLOBAL_PROPERTIES["+i+"] == void 0"));
}

test();
