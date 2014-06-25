/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.7.4.3-1.js
   ECMA Section:       15.7.4.3.1 Number.prototype.valueOf()
   Description:
   Returns this number value.

   The valueOf function is not generic; it generates a runtime error if its
   this value is not a Number object. Therefore it cannot be transferred to
   other kinds of objects for use as a method.

   Author:             christine@netscape.com
   Date:               16 september 1997
*/
var SECTION = "15.7.4.3-1";
var VERSION = "ECMA_1";
startTest();


writeHeaderToLog( SECTION + " Number.prototype.valueOf()");

//  the following two line causes navigator to crash -- cmb 9/16/97
new TestCase("SECTION",
	     "Number.prototype.valueOf()",       
	     0,       
	     eval("Number.prototype.valueOf()") );

new TestCase("SECTION",
	     "(new Number(1)).valueOf()",        
	     1,      
	     eval("(new Number(1)).valueOf()") );

new TestCase("SECTION",
	     "(new Number(-1)).valueOf()",       
	     -1,     
	     eval("(new Number(-1)).valueOf()") );

new TestCase("SECTION",
	     "(new Number(0)).valueOf()",        
	     0,      
	     eval("(new Number(0)).valueOf()") );

new TestCase("SECTION",
	     "(new Number(Number.POSITIVE_INFINITY)).valueOf()",
	     Number.POSITIVE_INFINITY,
	     eval("(new Number(Number.POSITIVE_INFINITY)).valueOf()") );

new TestCase("SECTION",
	     "(new Number(Number.NaN)).valueOf()", 
	     Number.NaN,
	     eval("(new Number(Number.NaN)).valueOf()") );

new TestCase("SECTION",
	     "(new Number()).valueOf()",        
	     0,      
	     eval("(new Number()).valueOf()") );

test();
