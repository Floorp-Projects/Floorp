/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.8.1.js
   ECMA Section:       15.8.1.js   Value Properties of the Math Object
   15.8.1.1    E
   15.8.1.2    LN10
   15.8.1.3    LN2
   15.8.1.4    LOG2E
   15.8.1.5    LOG10E
   15.8.1.6    PI
   15.8.1.7    SQRT1_2
   15.8.1.8    SQRT2
   Description:        verify the values of some math constants
   Author:             christine@netscape.com
   Date:               7 july 1997

*/
var SECTION = "15.8.1"
  var TITLE   = "Value Properties of the Math Object";

writeHeaderToLog( SECTION + " "+ TITLE);


new TestCase( "Math.E",            
	      2.7182818284590452354, 
	      Math.E );

new TestCase( "typeof Math.E",     
	      "number",              
	      typeof Math.E );

new TestCase( "Math.LN10",         
	      2.302585092994046,     
	      Math.LN10 );

new TestCase( "typeof Math.LN10",  
	      "number",              
	      typeof Math.LN10 );

new TestCase( "Math.LN2",         
	      0.6931471805599453,    
	      Math.LN2 );

new TestCase( "typeof Math.LN2",   
	      "number",              
	      typeof Math.LN2 );

new TestCase( "Math.LOG2E",        
	      1.4426950408889634,    
	      Math.LOG2E );

new TestCase( "typeof Math.LOG2E", 
	      "number",              
	      typeof Math.LOG2E );

new TestCase( "Math.LOG10E",       
	      0.4342944819032518,    
	      Math.LOG10E);

new TestCase( "typeof Math.LOG10E",
	      "number",              
	      typeof Math.LOG10E);

new TestCase( "Math.PI",           
	      3.14159265358979323846,
	      Math.PI );

new TestCase( "typeof Math.PI",    
	      "number",              
	      typeof Math.PI );

new TestCase( "Math.SQRT1_2",      
	      0.7071067811865476,    
	      Math.SQRT1_2);

new TestCase( "typeof Math.SQRT1_2",
	      "number",             
	      typeof Math.SQRT1_2);

new TestCase( "Math.SQRT2",        
	      1.4142135623730951,    
	      Math.SQRT2 );

new TestCase( "typeof Math.SQRT2", 
	      "number",              
	      typeof Math.SQRT2 );

new TestCase( "var MATHPROPS='';for( p in Math ){ MATHPROPS +=p; };MATHPROPS",
	      "",
	      eval("var MATHPROPS='';for( p in Math ){ MATHPROPS +=p; };MATHPROPS") );

test();
