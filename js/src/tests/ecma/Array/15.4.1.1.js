/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.4.1.1.js
   ECMA Section:       15.4.1 Array( item0, item1,... )

   Description:        When Array is called as a function rather than as a
   constructor, it creates and initializes a new array
   object.  Thus, the function call Array(...) is
   equivalent to the object creation new Array(...) with
   the same arguments.

   An array is created and returned as if by the expression
   new Array( item0, item1, ... ).

   Author:             christine@netscape.com
   Date:               7 october 1997
*/
var SECTION = "15.4.1.1";
var TITLE   = "Array Constructor Called as a Function";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "typeof Array(1,2)",       
	      "object",          
	      typeof Array(1,2) );

new TestCase( "(Array(1,2)).toString",   
	      Array.prototype.toString,   
	      (Array(1,2)).toString );

new TestCase( "var arr = Array(1,2,3); arr.toString = Object.prototype.toString; arr.toString()",
	      "[object Array]",
	      eval("var arr = Array(1,2,3); arr.toString = Object.prototype.toString; arr.toString()") );

new TestCase( "(Array(1,2)).length",     
	      2,                 
	      (Array(1,2)).length );

new TestCase( "var arr = (Array(1,2)); arr[0]", 
	      1,          
	      eval("var arr = (Array(1,2)); arr[0]") );

new TestCase( "var arr = (Array(1,2)); arr[1]", 
	      2,          
	      eval("var arr = (Array(1,2)); arr[1]") );

new TestCase( "var arr = (Array(1,2)); String(arr)", 
	      "1,2", 
	      eval("var arr = (Array(1,2)); String(arr)") );

test();

function ToUint32( n ) {
  n = Number( n );
  if( isNaN(n) || n == 0 || n == Number.POSITIVE_INFINITY ||
      n == Number.NEGATIVE_INFINITY ) {
    return 0;
  }
  var sign = n < 0 ? -1 : 1;

  return ( sign * ( n * Math.floor( Math.abs(n) ) ) ) % Math.pow(2, 32);
}

