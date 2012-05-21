/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.4.2.1-1.js
   ECMA Section:       15.4.2.1 new Array( item0, item1, ... )
   Description:        This description only applies of the constructor is
   given two or more arguments.

   The [[Prototype]] property of the newly constructed
   object is set to the original Array prototype object,
   the one that is the initial value of Array.prototype
   (15.4.3.1).

   The [[Class]] property of the newly constructed object
   is set to "Array".

   The length property of the newly constructed object is
   set to the number of arguments.

   The 0 property of the newly constructed object is set
   to item0... in general, for as many arguments as there
   are, the k property of the newly constructed object is
   set to argument k, where the first argument is
   considered to be argument number 0.

   This file tests the typeof the newly constructed object.

   Author:             christine@netscape.com
   Date:               7 october 1997
*/

var SECTION = "15.4.2.1-1";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "The Array Constructor:  new Array( item0, item1, ...)";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION,
	      "typeof new Array(1,2)",       
	      "object",          
	      typeof new Array(1,2) );

new TestCase( SECTION,
	      "(new Array(1,2)).toString",   
	      Array.prototype.toString,   
	      (new Array(1,2)).toString );

new TestCase( SECTION,
	      "var arr = new Array(1,2,3); arr.getClass = Object.prototype.toString; arr.getClass()",
	      "[object Array]",
	      eval("var arr = new Array(1,2,3); arr.getClass = Object.prototype.toString; arr.getClass()") );

new TestCase( SECTION,
	      "(new Array(1,2)).length",     
	      2,                 
	      (new Array(1,2)).length );

new TestCase( SECTION,
	      "var arr = (new Array(1,2)); arr[0]", 
	      1,          
	      eval("var arr = (new Array(1,2)); arr[0]") );

new TestCase( SECTION,
	      "var arr = (new Array(1,2)); arr[1]", 
	      2,          
	      eval("var arr = (new Array(1,2)); arr[1]") );

new TestCase( SECTION,
	      "var arr = (new Array(1,2)); String(arr)", 
	      "1,2", 
	      eval("var arr = (new Array(1,2)); String(arr)") );

test();
