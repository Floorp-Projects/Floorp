/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.7.4.2-4.js
   ECMA Section:       15.7.4.2.1 Number.prototype.toString()
   Description:
   If the radix is the number 10 or not supplied, then this number value is
   given as an argument to the ToString operator; the resulting string value
   is returned.

   If the radix is supplied and is an integer from 2 to 36, but not 10, the
   result is a string, the choice of which is implementation dependent.

   The toString function is not generic; it generates a runtime error if its
   this value is not a Number object. Therefore it cannot be transferred to
   other kinds of objects for use as a method.

   Author:             christine@netscape.com
   Date:               16 september 1997
*/
var SECTION = "15.7.4.2-4";

writeHeaderToLog( SECTION + " Number.prototype.toString()");

new TestCase(
	     "o = 3; o.toString = Number.prototype.toString; o.toString()",            
	     "3",   
	     eval("o = 3; o.toString = Number.prototype.toString; o.toString()") );

test();
