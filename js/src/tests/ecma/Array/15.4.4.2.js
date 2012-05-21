/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.4.4.2.js
   ECMA Section:       15.4.4.2 Array.prototype.toString()
   Description:        The elements of this object are converted to strings
   and these strings are then concatenated, separated by
   comma characters.  The result is the same as if the
   built-in join method were invoiked for this object
   with no argument.
   Author:             christine@netscape.com
   Date:               7 october 1997
*/

var SECTION = "15.4.4.2";
var VERSION = "ECMA_1";
startTest();
var TITLE   = "Array.prototype.toString";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( SECTION, 
	      "Array.prototype.toString.length", 
	      0, 
	      Array.prototype.toString.length );

new TestCase( SECTION, 
	      "(new Array()).toString()",    
	      "",    
	      (new Array()).toString() );

new TestCase( SECTION, 
	      "(new Array(2)).toString()",   
	      ",",   
	      (new Array(2)).toString() );

new TestCase( SECTION, 
	      "(new Array(0,1)).toString()", 
	      "0,1", 
	      (new Array(0,1)).toString() );

new TestCase( SECTION, 
	      "(new Array( Number.NaN, Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY)).toString()", 
	      "NaN,Infinity,-Infinity",  
	      (new Array( Number.NaN, Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY)).toString() );

new TestCase( SECTION, 
	      "(new Array( Boolean(1), Boolean(0))).toString()",  
	      "true,false",  
	      (new Array(Boolean(1),Boolean(0))).toString() );

new TestCase( SECTION, 
	      "(new Array(void 0,null)).toString()",   
	      ",",   
	      (new Array(void 0,null)).toString() );

var EXPECT_STRING = "";
var MYARR = new Array();

for ( var i = -50; i < 50; i+= 0.25 ) {
  MYARR[MYARR.length] = i;
  EXPECT_STRING += i +",";
}

EXPECT_STRING = EXPECT_STRING.substring( 0, EXPECT_STRING.length -1 );

new TestCase( SECTION,
	      "MYARR.toString()", 
	      EXPECT_STRING, 
	      MYARR.toString() );

test();
