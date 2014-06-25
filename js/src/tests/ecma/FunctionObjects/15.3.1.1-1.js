/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.3.1.1.js
   ECMA Section:       15.3.1.1 The Function Constructor Called as a Function

   Description:
   When the Function function is called with some arguments p1, p2, . . . , pn, body
   (where n might be 0, that is, there are no "p" arguments, and where body might
   also not be provided), the following steps are taken:

   1.  Create and return a new Function object exactly if the function constructor had
   been called with the same arguments (15.3.2.1).

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.3.1.1-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The Function Constructor Called as a Function";

writeHeaderToLog( SECTION + " "+ TITLE);

var MyObject = Function( "value", "this.value = value; this.valueOf =  Function( 'return this.value' ); this.toString =  Function( 'return String(this.value);' )" );


var myfunc = Function();
myfunc.toString = Object.prototype.toString;

//    not going to test toString here since it is implementation dependent.
//    new TestCase( SECTION,  "myfunc.toString()",     "function anonymous() { }",    myfunc.toString() );

myfunc.toString = Object.prototype.toString;
new TestCase(   SECTION,
		"myfunc = Function(); myfunc.toString = Object.prototype.toString; myfunc.toString()",
		"[object Function]",
		myfunc.toString() );

new TestCase( SECTION, 
	      "myfunc.length",                           
	      0,                     
	      myfunc.length );

new TestCase( SECTION, 
	      "myfunc.prototype.toString()",             
	      "[object Object]",     
	      myfunc.prototype.toString() );

new TestCase( SECTION, 
	      "myfunc.prototype.constructor",            
	      myfunc,                
	      myfunc.prototype.constructor );

new TestCase( SECTION, 
	      "myfunc.arguments",                        
	      null,                  
	      myfunc.arguments );

new TestCase( SECTION, 
	      "var OBJ = new MyObject(true); OBJ.valueOf()",   
	      true,            
	      eval("var OBJ = new MyObject(true); OBJ.valueOf()") );

new TestCase( SECTION, 
	      "OBJ.toString()",                          
	      "true",                
	      OBJ.toString() );

new TestCase( SECTION, 
	      "OBJ.toString = Object.prototype.toString; OBJ.toString()",
	      "[object Object]", 
	      eval("OBJ.toString = Object.prototype.toString; OBJ.toString()") );

new TestCase( SECTION, 
	      "MyObject.toString = Object.prototype.toString; MyObject.toString()",   
	      "[object Function]",  
	      eval("MyObject.toString = Object.prototype.toString; MyObject.toString()") );

new TestCase( SECTION, 
	      "MyObject.length",                             
	      1,     
	      MyObject.length );

new TestCase( SECTION, 
	      "MyObject.prototype.constructor",              
	      MyObject,  
	      MyObject.prototype.constructor );

new TestCase( SECTION, 
	      "MyObject.arguments",                          
	      null,  
	      MyObject.arguments );
   
test();


