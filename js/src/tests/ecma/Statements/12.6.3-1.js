/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          12.6.3-1.js
   ECMA Section:       12.6.3 The for...in Statement
   Description:

   Author:             christine@netscape.com
   Date:               11 september 1997
*/

var SECTION = "12.6.3-1";
var TITLE   = "The for..in statement";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "var x; Number.prototype.foo = 34; for ( j in 7 ) x = j; x",
	      "foo",
	      eval("var x; Number.prototype.foo = 34; for ( j in 7 ){x = j;} x") );

test();

