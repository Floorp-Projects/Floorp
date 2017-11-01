/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.5.3.1.js
   ECMA Section:       15.5.3 Properties of the String Constructor

   Description:	    The value of the internal [[Prototype]] property of
   the String constructor is the Function prototype
   object.

   In addition to the internal [[Call]] and [[Construct]]
   properties, the String constructor also has the length
   property, as well as properties described in 15.5.3.1
   and 15.5.3.2.

   Author:             christine@netscape.com
   Date:               1 october 1997
*/

var SECTION = "15.5.3";
var passed = true;
writeHeaderToLog( SECTION + " Properties of the String Constructor" );

new TestCase( "String.prototype",             Function.prototype,     String.__proto__ );

test();
