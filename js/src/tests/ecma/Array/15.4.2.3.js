/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.4.2.3.js
   ECMA Section:       15.4.2.3 new Array()
   Description:        The [[Prototype]] property of the newly constructed
   object is set to the origianl Array prototype object,
   the one that is the initial value of Array.prototype.
   The [[Class]] property of the new object is set to
   "Array".  The length of the object is set to 0.

   Author:             christine@netscape.com
   Date:               7 october 1997
*/

var SECTION = "15.4.2.3";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The Array Constructor:  new Array()";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "new   Array() +''",       
	      "",                
	      (new Array()) +"" );

new TestCase( SECTION,
	      "typeof new Array()",      
	      "object",          
	      (typeof new Array()) );

new TestCase( SECTION,
	      "var arr = new Array(); arr.getClass = Object.prototype.toString; arr.getClass()",
	      "[object Array]",
	      eval("var arr = new Array(); arr.getClass = Object.prototype.toString; arr.getClass()") );

new TestCase( SECTION,
	      "(new Array()).length",    
	      0,                 
	      (new Array()).length );

new TestCase( SECTION,
	      "(new Array()).toString == Array.prototype.toString",  
	      true,      
	      (new Array()).toString == Array.prototype.toString );

new TestCase( SECTION,
	      "(new Array()).join  == Array.prototype.join",         
	      true,      
	      (new Array()).join  == Array.prototype.join );

new TestCase( SECTION,
	      "(new Array()).reverse == Array.prototype.reverse",    
	      true,      
	      (new Array()).reverse  == Array.prototype.reverse );

new TestCase( SECTION,
	      "(new Array()).sort  == Array.prototype.sort",        
	      true,      
	      (new Array()).sort  == Array.prototype.sort );

test();
