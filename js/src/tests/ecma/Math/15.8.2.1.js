/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.8.2.1.js
   ECMA Section:       15.8.2.1 abs( x )
   Description:        return the absolute value of the argument,
   which should be the magnitude of the argument
   with a positive sign.
   -   if x is NaN, return NaN
   -   if x is -0, result is +0
   -   if x is -Infinity, result is +Infinity
   Author:             christine@netscape.com
   Date:               7 july 1997
*/
var SECTION = "15.8.2.1";
var TITLE   = "Math.abs()";
var BUGNUMBER = "77391";
printBugNumber(BUGNUMBER);

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase(
	      "Math.abs.length",
	      1,
              Math.abs.length );

new TestCase(
	      "Math.abs()", 
	      Number.NaN,
	      Math.abs() );

new TestCase(
	      "Math.abs( void 0 )", 
	      Number.NaN,
	      Math.abs(void 0) );

new TestCase(
	      "Math.abs( null )",  
	      0,   
	      Math.abs(null) );

new TestCase(
	      "Math.abs( true )", 
	      1,    
	      Math.abs(true) );

new TestCase(
	      "Math.abs( false )", 
	      0,     
	      Math.abs(false) );

new TestCase(
	      "Math.abs( string primitive)",
	      Number.NaN, 
	      Math.abs("a string primitive") );

new TestCase(
	      "Math.abs( string object )",  
	      Number.NaN,    
	      Math.abs(new String( 'a String object' ))  );

new TestCase(
	      "Math.abs( Number.NaN )", 
	      Number.NaN,
	      Math.abs(Number.NaN) );

new TestCase(
	      "Math.abs(0)", 
	      0,
              Math.abs( 0 ) );

new TestCase(
	      "Math.abs( -0 )", 
	      0,  
	      Math.abs(-0) );

new TestCase(
	      "Infinity/Math.abs(-0)",
	      Infinity, 
	      Infinity/Math.abs(-0) );

new TestCase(
	      "Math.abs( -Infinity )",      
	      Number.POSITIVE_INFINITY,  
	      Math.abs( Number.NEGATIVE_INFINITY ) );

new TestCase(
	      "Math.abs( Infinity )",  
	      Number.POSITIVE_INFINITY,
	      Math.abs( Number.POSITIVE_INFINITY ) );

new TestCase(
	      "Math.abs( - MAX_VALUE )",   
	      Number.MAX_VALUE,
	      Math.abs( - Number.MAX_VALUE )       );

new TestCase(
	      "Math.abs( - MIN_VALUE )",
	      Number.MIN_VALUE,
	      Math.abs( -Number.MIN_VALUE )        );

new TestCase(
	      "Math.abs( MAX_VALUE )",  
	      Number.MAX_VALUE,  
	      Math.abs( Number.MAX_VALUE )       );

new TestCase(
	      "Math.abs( MIN_VALUE )",
	      Number.MIN_VALUE, 
	      Math.abs( Number.MIN_VALUE )        );

new TestCase(
	      "Math.abs( -1 )",    
	      1,   
	      Math.abs( -1 )                       );

new TestCase(
	      "Math.abs( new Number( -1 ) )",
	      1,   
	      Math.abs( new Number(-1) )           );

new TestCase(
	      "Math.abs( 1 )",  
	      1, 
	      Math.abs( 1 ) );

new TestCase(
	      "Math.abs( Math.PI )", 
	      Math.PI,   
	      Math.abs( Math.PI ) );

new TestCase(
	      "Math.abs( -Math.PI )", 
	      Math.PI,  
	      Math.abs( -Math.PI ) );

new TestCase(
	      "Math.abs(-1/100000000)",
	      1/100000000,  
	      Math.abs(-1/100000000) );

new TestCase(
	      "Math.abs(-Math.pow(2,32))", 
	      Math.pow(2,32),    
	      Math.abs(-Math.pow(2,32)) );

new TestCase(
	      "Math.abs(Math.pow(2,32))",
	      Math.pow(2,32), 
	      Math.abs(Math.pow(2,32)) );

new TestCase(
	      "Math.abs( -0xfff )", 
	      4095,    
	      Math.abs( -0xfff ) );

new TestCase(
	      "Math.abs( -0777 )", 
	      511,   
	      Math.abs(-0777 ) );

new TestCase(
	      "Math.abs('-1e-1')",  
	      0.1,  
	      Math.abs('-1e-1') );

new TestCase(
	      "Math.abs('0xff')",  
	      255,  
	      Math.abs('0xff') );

new TestCase(
	      "Math.abs('077')",   
	      77,   
	      Math.abs('077') );

new TestCase(
	      "Math.abs( 'Infinity' )",
	      Infinity,
	      Math.abs('Infinity') );

new TestCase(
	      "Math.abs( '-Infinity' )",
	      Infinity,
	      Math.abs('-Infinity') );

test();
