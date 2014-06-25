/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.7.4.2.js
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
var SECTION = "15.7.4.2-1";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " Number.prototype.toString()");

//  the following two lines cause navigator to crash -- cmb 9/16/97
new TestCase(SECTION,
	     "Number.prototype.toString()",      
	     "0",       
	     eval("Number.prototype.toString()") );

new TestCase(SECTION,
	     "typeof(Number.prototype.toString())",
	     "string",     
	     eval("typeof(Number.prototype.toString())") );

new TestCase(SECTION, 
	     "s = Number.prototype.toString; o = new Number(); o.toString = s; o.toString()", 
	     "0",         
	     eval("s = Number.prototype.toString; o = new Number(); o.toString = s; o.toString()") );

new TestCase(SECTION, 
	     "s = Number.prototype.toString; o = new Number(1); o.toString = s; o.toString()",
	     "1",         
	     eval("s = Number.prototype.toString; o = new Number(1); o.toString = s; o.toString()") );

new TestCase(SECTION, 
	     "s = Number.prototype.toString; o = new Number(-1); o.toString = s; o.toString()",
	     "-1",        
	     eval("s = Number.prototype.toString; o = new Number(-1); o.toString = s; o.toString()") );

new TestCase(SECTION,
	     "var MYNUM = new Number(255); MYNUM.toString(10)",         
	     "255",     
	     eval("var MYNUM = new Number(255); MYNUM.toString(10)") );

new TestCase(SECTION,
	     "var MYNUM = new Number(Number.NaN); MYNUM.toString(10)",  
	     "NaN",     
	     eval("var MYNUM = new Number(Number.NaN); MYNUM.toString(10)") );

new TestCase(SECTION,
	     "var MYNUM = new Number(Infinity); MYNUM.toString(10)",  
	     "Infinity",  
	     eval("var MYNUM = new Number(Infinity); MYNUM.toString(10)") );

new TestCase(SECTION,
	     "var MYNUM = new Number(-Infinity); MYNUM.toString(10)",  
	     "-Infinity",
	     eval("var MYNUM = new Number(-Infinity); MYNUM.toString(10)") );

test();
