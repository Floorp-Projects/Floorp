/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.4.4.3-1.js
   ECMA Section:       15.4.4.3-1 Array.prototype.reverse()
   Description:

   The elements of the array are rearranged so as to reverse their order.
   This object is returned as the result of the call.

   1.   Call the [[Get]] method of this object with argument "length".
   2.   Call ToUint32(Result(1)).
   3.   Compute floor(Result(2)/2).
   4.   Let k be 0.
   5.   If k equals Result(3), return this object.
   6.   Compute Result(2)k1.
   7.   Call ToString(k).
   8.   ToString(Result(6)).
   9.   Call the [[Get]] method of this object with argument Result(7).
   10.   Call the [[Get]] method of this object with argument Result(8).
   11.   If this object has a property named by Result(8), go to step 12; but
   if this object has no property named by Result(8), then go to either
   step 12 or step 14, depending on the implementation.
   12.   Call the [[Put]] method of this object with arguments Result(7) and
   Result(10).
   13.   Go to step 15.
   14.   Call the [[Delete]] method on this object, providing Result(7) as the
   name of the property to delete.
   15.   If this object has a property named by Result(7), go to step 16; but if
   this object has no property named by Result(7), then go to either step 16
   or step 18, depending on the implementation.
   16.   Call the [[Put]] method of this object with arguments Result(8) and
   Result(9).
   17.   Go to step 19.
   18.   Call the [[Delete]] method on this object, providing Result(8) as the
   name of the property to delete.
   19.   Increase k by 1.
   20.   Go to step 5.

   Note that the reverse function is intentionally generic; it does not require
   that its this value be an Array object. Therefore it can be transferred to other
   kinds of objects for use as a method. Whether the reverse function can be applied
   successfully to a host object is implementation dependent.

   Note:   Array.prototype.reverse allows some flexibility in implementation
   regarding array indices that have not been populated. This test covers the
   cases in which unpopulated indices are not deleted, since the JavaScript
   implementation does not delete uninitialzed indices.

   Author:             christine@netscape.com
   Date:               7 october 1997
*/

var SECTION = "15.4.4.4-1";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " Array.prototype.reverse()");

var ARR_PROTOTYPE = Array.prototype;

new TestCase( SECTION, "Array.prototype.reverse.length",           0,      Array.prototype.reverse.length );

// length of array is 0
new TestCase(   SECTION,
		"var A = new Array();   A.reverse(); A.length",
		0,
		eval("var A = new Array();   A.reverse(); A.length") );

test();

function CheckItems( R, A ) {
  for ( var i = 0; i < R.length; i++ ) {
    new TestCase(
      SECTION,
      "A["+i+ "]",
      R[i],
      A[i] );
  }
}
test();

function Object_1( value ) {
  this.array = value.split(",");
  this.length = this.array.length;
  for ( var i = 0; i < this.length; i++ ) {
    this[i] = eval(this.array[i]);
  }
  this.join = Array.prototype.reverse;
  this.getClass = Object.prototype.toString;
}
function Reverse( array ) {
  var r2 = array.length;
  var k = 0;
  var r3 = Math.floor( r2/2 );
  if ( r3 == k ) {
    return array;
  }

  for ( k = 0;  k < r3; k++ ) {
    var r6 = r2 - k - 1;
//        var r7 = String( k );
    var r7 = k;
    var r8 = String( r6 );

    var r9 = array[r7];
    var r10 = array[r8];

    array[r7] = r10;
    array[r8] = r9;
  }

  return array;
}
function Iterate( array ) {
  for ( var i = 0; i < array.length; i++ ) {
//        print( i+": "+ array[String(i)] );
  }
}

function Object_1( value ) {
  this.array = value.split(",");
  this.length = this.array.length;
  for ( var i = 0; i < this.length; i++ ) {
    this[i] = this.array[i];
  }
  this.reverse = Array.prototype.reverse;
  this.getClass = Object.prototype.toString;
}
