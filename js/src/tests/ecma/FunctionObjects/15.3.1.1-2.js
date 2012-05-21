/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.3.1.1-2.js
   ECMA Section:       15.3.1.1 The Function Constructor Called as a Function
   Function(p1, p2, ..., pn, body )

   Description:
   When the Function function is called with some arguments p1, p2, . . . , pn,
   body (where n might be 0, that is, there are no "p" arguments, and where body
   might also not be provided), the following steps are taken:

   1.  Create and return a new Function object exactly if the function constructor
   had been called with the same arguments (15.3.2.1).

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.3.1.1-2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The Function Constructor Called as a Function";

writeHeaderToLog( SECTION + " "+ TITLE);

var myfunc1 =  Function("a","b","c", "return a+b+c" );
var myfunc2 =  Function("a, b, c",   "return a+b+c" );
var myfunc3 =  Function("a,b", "c",  "return a+b+c" );

myfunc1.toString = Object.prototype.toString;
myfunc2.toString = Object.prototype.toString;
myfunc3.toString = Object.prototype.toString;

new TestCase( SECTION, 
	      "myfunc1 =  Function('a','b','c'); myfunc.toString = Object.prototype.toString; myfunc.toString()",
	      "[object Function]",
	      myfunc1.toString() );

new TestCase( SECTION, 
	      "myfunc1.length",                           
	      3,                     
	      myfunc1.length );

new TestCase( SECTION, 
	      "myfunc1.prototype.toString()",             
	      "[object Object]",     
	      myfunc1.prototype.toString() );

new TestCase( SECTION, 
	      "myfunc1.prototype.constructor",            
	      myfunc1,               
	      myfunc1.prototype.constructor );

new TestCase( SECTION, 
	      "myfunc1.arguments",                        
	      null,                  
	      myfunc1.arguments );

new TestCase( SECTION, 
	      "myfunc1(1,2,3)",                           
	      6,                     
	      myfunc1(1,2,3) );

new TestCase( SECTION, 
	      "var MYPROPS = ''; for ( var p in myfunc1.prototype ) { MYPROPS += p; }; MYPROPS",
	      "",
	      eval("var MYPROPS = ''; for ( var p in myfunc1.prototype ) { MYPROPS += p; }; MYPROPS") );

new TestCase( SECTION, 
	      "myfunc2 =  Function('a','b','c'); myfunc.toString = Object.prototype.toString; myfunc.toString()",
	      "[object Function]",
	      myfunc2.toString() );

new TestCase( SECTION, 
	      "myfunc2.length",                           
	      3,                     
	      myfunc2.length );

new TestCase( SECTION, 
	      "myfunc2.prototype.toString()",             
	      "[object Object]",     
	      myfunc2.prototype.toString() );

new TestCase( SECTION, 
	      "myfunc2.prototype.constructor",            
	      myfunc2,                
	      myfunc2.prototype.constructor );

new TestCase( SECTION, 
	      "myfunc2.arguments",                        
	      null,                  
	      myfunc2.arguments );

new TestCase( SECTION, 
	      "myfunc2( 1000, 200, 30 )",                
	      1230,                   
	      myfunc2(1000,200,30) );

new TestCase( SECTION, 
	      "var MYPROPS = ''; for ( var p in myfunc2.prototype ) { MYPROPS += p; }; MYPROPS",
	      "",
	      eval("var MYPROPS = ''; for ( var p in myfunc2.prototype ) { MYPROPS += p; }; MYPROPS") );

new TestCase( SECTION, 
	      "myfunc3 =  Function('a','b','c'); myfunc.toString = Object.prototype.toString; myfunc.toString()",
	      "[object Function]",
	      myfunc3.toString() );

new TestCase( SECTION, 
	      "myfunc3.length",                           
	      3,                     
	      myfunc3.length );

new TestCase( SECTION, 
	      "myfunc3.prototype.toString()",             
	      "[object Object]",     
	      myfunc3.prototype.toString() );

new TestCase( SECTION, 
	      "myfunc3.prototype.valueOf() +''",          
	      "[object Object]",     
	      myfunc3.prototype.valueOf() +'' );

new TestCase( SECTION, 
	      "myfunc3.prototype.constructor",            
	      myfunc3,                
	      myfunc3.prototype.constructor );

new TestCase( SECTION, 
	      "myfunc3.arguments",                        
	      null,                  
	      myfunc3.arguments );

new TestCase( SECTION, 
	      "myfunc3(-100,100,NaN)",                   
	      Number.NaN,             
	      myfunc3(-100,100,NaN) );

new TestCase( SECTION, 
	      "var MYPROPS = ''; for ( var p in myfunc3.prototype ) { MYPROPS += p; }; MYPROPS",
	      "",
	      eval("var MYPROPS = ''; for ( var p in myfunc3.prototype ) { MYPROPS += p; }; MYPROPS") );

test();
