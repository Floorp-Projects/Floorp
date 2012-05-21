/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          10.2.3-1.js
   ECMA Section:       10.2.3 Function and Anonymous Code
   Description:

   The scope chain is initialized to contain the activation object followed
   by the global object. Variable instantiation is performed using the
   activation by the global object. Variable instantiation is performed using
   the activation object as the variable object and using property attributes
   { DontDelete }. The caller provides the this value. If the this value
   provided by the caller is not an object (including the case where it is
   null), then the this value is the global object.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "10.2.3-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Eval Code";

writeHeaderToLog( SECTION + " "+ TITLE);

var o = new MyObject("hello")

  new TestCase( SECTION,
		"var o = new MyObject('hello'); o.THIS == x",
		true,
		o.THIS == o );

var o = MyFunction();

new TestCase( SECTION,
	      "var o = MyFunction(); o == this",
	      true,
	      o == this );

test();

function MyFunction( value ) {
  return this;
}
function MyObject( value ) {
  this.THIS = this;
}
