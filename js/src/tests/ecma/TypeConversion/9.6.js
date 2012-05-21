/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          9.6.js
   ECMA Section:       9.6  Type Conversion:  ToUint32
   Description:        rules for converting an argument to an unsigned
   32 bit integer

   this test uses >>> 0 to convert the argument to
   an unsigned 32bit integer.

   1 call ToNumber on argument
   2 if result is NaN, 0, -0, Infinity, -Infinity
   return 0
   3 compute (sign (result(1)) * floor(abs(result 1)))
   4 compute result(3) modulo 2^32:
   5 return result(4)

   special cases:
   -0          returns 0
   Infinity    returns 0
   -Infinity   returns 0
   0           returns 0
   ToInt32(ToUint32(x)) == ToInt32(x) for all values of x
   ** NEED TO DO THIS PART IN A SEPARATE TEST FILE **


   Author:             christine@netscape.com
   Date:               17 july 1997
*/

var SECTION = "9.6";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " Type Conversion:  ToUint32");

new TestCase( SECTION,    "0 >>> 0",                          0,          0 >>> 0 );
//    new TestCase( SECTION,    "+0 >>> 0",                         0,          +0 >>> 0);
new TestCase( SECTION,    "-0 >>> 0",                         0,          -0 >>> 0 );
new TestCase( SECTION,    "'Infinity' >>> 0",                 0,          "Infinity" >>> 0 );
new TestCase( SECTION,    "'-Infinity' >>> 0",                0,          "-Infinity" >>> 0);
new TestCase( SECTION,    "'+Infinity' >>> 0",                0,          "+Infinity" >>> 0 );
new TestCase( SECTION,    "Number.POSITIVE_INFINITY >>> 0",   0,          Number.POSITIVE_INFINITY >>> 0 );
new TestCase( SECTION,    "Number.NEGATIVE_INFINITY >>> 0",   0,          Number.NEGATIVE_INFINITY >>> 0 );
new TestCase( SECTION,    "Number.NaN >>> 0",                 0,          Number.NaN >>> 0 );

new TestCase( SECTION,    "Number.MIN_VALUE >>> 0",           0,          Number.MIN_VALUE >>> 0 );
new TestCase( SECTION,    "-Number.MIN_VALUE >>> 0",          0,          Number.MIN_VALUE >>> 0 );
new TestCase( SECTION,    "0.1 >>> 0",                        0,          0.1 >>> 0 );
new TestCase( SECTION,    "-0.1 >>> 0",                       0,          -0.1 >>> 0 );
new TestCase( SECTION,    "1 >>> 0",                          1,          1 >>> 0 );
new TestCase( SECTION,    "1.1 >>> 0",                        1,          1.1 >>> 0 );

new TestCase( SECTION,    "-1.1 >>> 0",                       ToUint32(-1.1),       -1.1 >>> 0 );
new TestCase( SECTION,    "-1 >>> 0",                         ToUint32(-1),         -1 >>> 0 );

new TestCase( SECTION,    "2147483647 >>> 0",         ToUint32(2147483647),     2147483647 >>> 0 );
new TestCase( SECTION,    "2147483648 >>> 0",         ToUint32(2147483648),     2147483648 >>> 0 );
new TestCase( SECTION,    "2147483649 >>> 0",         ToUint32(2147483649),     2147483649 >>> 0 );

new TestCase( SECTION,    "4294967295 >>> 0",         ToUint32(4294967295),     4294967295 >>> 0 );
new TestCase( SECTION,    "4294967296 >>> 0",         ToUint32(4294967296),     4294967296 >>> 0 );
new TestCase( SECTION,    "4294967297 >>> 0",         ToUint32(4294967297),     4294967297 >>> 0 );

new TestCase( SECTION,    "-2147483647 >>> 0",        ToUint32(-2147483647),    -2147483647 >>> 0 );
new TestCase( SECTION,    "-2147483648 >>> 0",        ToUint32(-2147483648),    -2147483648 >>> 0 );
new TestCase( SECTION,    "-2147483649 >>> 0",        ToUint32(-2147483649),    -2147483649 >>> 0 );

new TestCase( SECTION,    "-4294967295 >>> 0",        ToUint32(-4294967295),    -4294967295 >>> 0 );
new TestCase( SECTION,    "-4294967296 >>> 0",        ToUint32(-4294967296),    -4294967296 >>> 0 );
new TestCase( SECTION,    "-4294967297 >>> 0",        ToUint32(-4294967297),    -4294967297 >>> 0 );

new TestCase( SECTION,    "'2147483647' >>> 0",       ToUint32(2147483647),     '2147483647' >>> 0 );
new TestCase( SECTION,    "'2147483648' >>> 0",       ToUint32(2147483648),     '2147483648' >>> 0 );
new TestCase( SECTION,    "'2147483649' >>> 0",       ToUint32(2147483649),     '2147483649' >>> 0 );

new TestCase( SECTION,    "'4294967295' >>> 0",       ToUint32(4294967295),     '4294967295' >>> 0 );
new TestCase( SECTION,    "'4294967296' >>> 0",       ToUint32(4294967296),     '4294967296' >>> 0 );
new TestCase( SECTION,    "'4294967297' >>> 0",       ToUint32(4294967297),     '4294967297' >>> 0 );


test();

function ToUint32( n ) {
  n = Number( n );
  var sign = ( n < 0 ) ? -1 : 1;

  if ( Math.abs( n ) == 0 || Math.abs( n ) == Number.POSITIVE_INFINITY) {
    return 0;
  }
  n = sign * Math.floor( Math.abs(n) )

    n = n % Math.pow(2,32);

  if ( n < 0 ){
    n += Math.pow(2,32);
  }

  return ( n );
}

