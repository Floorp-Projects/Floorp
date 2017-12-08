/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.2.3.1-1.js
   ECMA Section:       15.2.3.1 Object.prototype

   Description:        The initial value of Object.prototype is the built-in
   Object prototype object.

   This property shall have the attributes [ DontEnum,
   DontDelete ReadOnly ]

   This tests the [DontEnum] property of Object.prototype

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.2.3.1-1";
var TITLE   = "Object.prototype";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "var str = '';for ( p in Object ) { str += p; }; str",
	      "",
	      eval( "var str = ''; for ( p in Object ) { str += p; }; str" ) );

test();
