/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.3.2.1.js
   ECMA Section:       15.3.2.1 The Function Constructor
   new Function(p1, p2, ..., pn, body )

   Description:        The last argument specifies the body (executable code)
   of a function; any preceding arguments sepcify formal
   parameters.

   See the text for description of this section.

   This test examples from the specification.

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.3.2.1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The Function Constructor";

writeHeaderToLog( SECTION + " "+ TITLE);

var MyObject = new Function( "value", "this.value = value; this.valueOf = new Function( 'return this.value' ); this.toString = new Function( 'return String(this.value);' )" );

new TestCase( SECTION,
	      "MyObject.__proto__ == Function.prototype",   
	      true,
	      MyObject.__proto__ == Function.prototype );

test();
