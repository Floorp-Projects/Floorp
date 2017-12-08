/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.4.3.1-1.js
   ECMA Section:       15.4.3.1 Array.prototype
   Description:        The initial value of Array.prototype is the built-in
   Array prototype object (15.4.4).

   Author:             christine@netscape.com
   Date:               7 october 1997
*/

var SECTION = "15.4.3.1-1";
var TITLE   = "Array.prototype";

writeHeaderToLog( SECTION + " "+ TITLE);


var ARRAY_PROTO = Array.prototype;

new TestCase( "var props = ''; for ( p in Array  ) { props += p } props",
	      "",
	      eval("var props = ''; for ( p in Array  ) { props += p } props") );

new TestCase( "Array.prototype = null; Array.prototype",  
	      ARRAY_PROTO,
	      eval("Array.prototype = null; Array.prototype") );

new TestCase( "delete Array.prototype",                  
	      false,      
	      delete Array.prototype );

new TestCase( "delete Array.prototype; Array.prototype", 
	      ARRAY_PROTO,
	      eval("delete Array.prototype; Array.prototype") );

test();
