/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.4.2.2-2.js
   ECMA Section:       15.4.2.2 new Array(len)

   Description:        This description only applies of the constructor is
   given two or more arguments.

   The [[Prototype]] property of the newly constructed
   object is set to the original Array prototype object,
   the one that is the initial value of Array.prototype(0)
   (15.4.3.1).

   The [[Class]] property of the newly constructed object
   is set to "Array".

   If the argument len is a number, then the length
   property  of the newly constructed object is set to
   ToUint32(len).

   If the argument len is not a number, then the length
   property of the newly constructed object is set to 1
   and the 0 property of the newly constructed object is
   set to len.

   This file tests length of the newly constructed array
   when len is not a number.

   Author:             christine@netscape.com
   Date:               7 october 1997
*/
var SECTION = "15.4.2.2-2";
var TITLE   = "The Array Constructor:  new Array( len )";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "(new Array(new Number(1073741823))).length",  
	      1,     
	      (new Array(new Number(1073741823))).length );

new TestCase( "(new Array(new Number(0))).length",           
	      1,     
	      (new Array(new Number(0))).length );

new TestCase( "(new Array(new Number(1000))).length",        
	      1,     
	      (new Array(new Number(1000))).length );

new TestCase( "(new Array('mozilla, larryzilla, curlyzilla')).length",
	      1, 
	      (new Array('mozilla, larryzilla, curlyzilla')).length );

new TestCase( "(new Array(true)).length",                    
	      1,     
	      (new Array(true)).length );

new TestCase( "(new Array(false)).length",                   
	      1,     
	      (new Array(false)).length);

new TestCase( "(new Array(new Boolean(true)).length",        
	      1,     
	      (new Array(new Boolean(true))).length );

new TestCase( "(new Array(new Boolean(false)).length",       
	      1,     
	      (new Array(new Boolean(false))).length );

test();
