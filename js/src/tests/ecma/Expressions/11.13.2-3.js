/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          11.13.2-4.js
   ECMA Section:       11.13.2 Compound Assignment: %=
   Description:

   *= /= %= += -= <<= >>= >>>= &= ^= |=

   11.13.2 Compound assignment ( op= )

   The production AssignmentExpression :
   LeftHandSideExpression @ = AssignmentExpression, where @ represents one of
   the operators indicated above, is evaluated as follows:

   1.  Evaluate LeftHandSideExpression.
   2.  Call GetValue(Result(1)).
   3.  Evaluate AssignmentExpression.
   4.  Call GetValue(Result(3)).
   5.  Apply operator @ to Result(2) and Result(4).
   6.  Call PutValue(Result(1), Result(5)).
   7.  Return Result(5).

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.13.2-3";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " Compound Assignment: +=");

// If either operand is NaN,  result is NaN

new TestCase( SECTION,   
              "VAR1 = NaN; VAR2=1; VAR1 %= VAR2",      
              Number.NaN,
              eval("VAR1 = Number.NaN; VAR2=1; VAR1 %= VAR2") );

new TestCase( SECTION,   
              "VAR1 = NaN; VAR2=1; VAR1 %= VAR2; VAR1",
              Number.NaN,
              eval("VAR1 = Number.NaN; VAR2=1; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = NaN; VAR2=0; VAR1 %= VAR2",      
              Number.NaN,
              eval("VAR1 = Number.NaN; VAR2=0; VAR1 %= VAR2") );

new TestCase( SECTION,   
              "VAR1 = NaN; VAR2=0; VAR1 %= VAR2; VAR1",
              Number.NaN,
              eval("VAR1 = Number.NaN; VAR2=0; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = 0; VAR2=NaN; VAR1 %= VAR2",      
              Number.NaN,
              eval("VAR1 = 0; VAR2=Number.NaN; VAR1 %= VAR2") );

new TestCase( SECTION,   
              "VAR1 = 0; VAR2=NaN; VAR1 %= VAR2; VAR1",
              Number.NaN,
              eval("VAR1 = 0; VAR2=Number.NaN; VAR1 %= VAR2; VAR1") );

// if the dividend is infinity or the divisor is zero or both, the result is NaN

new TestCase( SECTION,   
              "VAR1 = Infinity; VAR2= Infinity; VAR1 %= VAR2; VAR1",  
              Number.NaN,     
              eval("VAR1 = Number.POSITIVE_INFINITY; VAR2 = Number.POSITIVE_INFINITY; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = Infinity; VAR2= -Infinity; VAR1 %= VAR2; VAR1", 
              Number.NaN,     
              eval("VAR1 = Number.POSITIVE_INFINITY; VAR2 = Number.NEGATIVE_INFINITY; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 =-Infinity; VAR2= Infinity; VAR1 %= VAR2; VAR1",  
              Number.NaN,     
              eval("VAR1 = Number.NEGATIVE_INFINITY; VAR2 = Number.POSITIVE_INFINITY; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 =-Infinity; VAR2=-Infinity; VAR1 %= VAR2; VAR1",  
              Number.NaN,     
              eval("VAR1 = Number.NEGATIVE_INFINITY; VAR2 = Number.NEGATIVE_INFINITY; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = 0; VAR2= Infinity; VAR2 %= VAR1",   
              Number.NaN,     
              eval("VAR1 = 0; VAR2 = Number.POSITIVE_INFINITY; VAR2 %= VAR1; VAR2") );

new TestCase( SECTION,   
              "VAR1 = -0; VAR2= Infinity; VAR2 %= VAR1",  
              Number.NaN,     
              eval("VAR1 = -0; VAR2 = Number.POSITIVE_INFINITY; VAR2 %= VAR1; VAR2") );

new TestCase( SECTION,   
              "VAR1 = -0; VAR2= -Infinity; VAR2 %= VAR1", 
              Number.NaN,     
              eval("VAR1 = -0; VAR2 = Number.NEGATIVE_INFINITY; VAR2 %= VAR1; VAR2") );

new TestCase( SECTION,   
              "VAR1 = 0; VAR2= -Infinity; VAR2 %= VAR1",  
              Number.NaN,     
              eval("VAR1 = 0; VAR2 = Number.NEGATIVE_INFINITY; VAR2 %= VAR1; VAR2") );

new TestCase( SECTION,   
              "VAR1 = 1; VAR2= Infinity; VAR2 %= VAR1",   
              Number.NaN,     
              eval("VAR1 = 1; VAR2 = Number.POSITIVE_INFINITY; VAR2 %= VAR1; VAR2") );

new TestCase( SECTION,   
              "VAR1 = -1; VAR2= Infinity; VAR2 %= VAR1",  
              Number.NaN,     
              eval("VAR1 = -1; VAR2 = Number.POSITIVE_INFINITY; VAR2 %= VAR1; VAR2") );

new TestCase( SECTION,   
              "VAR1 = -1; VAR2= -Infinity; VAR2 %= VAR1", 
              Number.NaN,     
              eval("VAR1 = -1; VAR2 = Number.NEGATIVE_INFINITY; VAR2 %= VAR1; VAR2") );

new TestCase( SECTION,   
              "VAR1 = 1; VAR2= -Infinity; VAR2 %= VAR1",  
              Number.NaN,     
              eval("VAR1 = 1; VAR2 = Number.NEGATIVE_INFINITY; VAR2 %= VAR1; VAR2") );

new TestCase( SECTION,   
              "VAR1 = 0; VAR2= 0; VAR1 %= VAR2",   
              Number.NaN,     
              eval("VAR1 = 0; VAR2 = 0; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = 0; VAR2= -0; VAR1 %= VAR2",  
              Number.NaN,     
              eval("VAR1 = 0; VAR2 = -0; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = -0; VAR2= 0; VAR1 %= VAR2",  
              Number.NaN,     
              eval("VAR1 = -0; VAR2 = 0; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = -0; VAR2= -0; VAR1 %= VAR2", 
              Number.NaN,     
              eval("VAR1 = -0; VAR2 = -0; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = 1; VAR2= 0; VAR1 %= VAR2",   
              Number.NaN,     
              eval("VAR1 = 1; VAR2 = 0; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = 1; VAR2= -0; VAR1 %= VAR2",  
              Number.NaN,     
              eval("VAR1 = 1; VAR2 = -0; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = -1; VAR2= 0; VAR1 %= VAR2",  
              Number.NaN,     
              eval("VAR1 = -1; VAR2 = 0; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = -1; VAR2= -0; VAR1 %= VAR2", 
              Number.NaN,     
              eval("VAR1 = -1; VAR2 = -0; VAR1 %= VAR2; VAR1") );

// if the dividend is finite and the divisor is an infinity, the result equals the dividend.

new TestCase( SECTION,   
              "VAR1 = 0; VAR2= Infinity; VAR1 %= VAR2;VAR1",   
              0,     
              eval("VAR1 = 0; VAR2 = Number.POSITIVE_INFINITY; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = -0; VAR2= Infinity; VAR1 %= VAR2;VAR1",  
              -0,    
              eval("VAR1 = -0; VAR2 = Number.POSITIVE_INFINITY; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = -0; VAR2= -Infinity; VAR1 %= VAR2;VAR1", 
              -0,    
              eval("VAR1 = -0; VAR2 = Number.NEGATIVE_INFINITY; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = 0; VAR2= -Infinity; VAR1 %= VAR2;VAR1",  
              0,     
              eval("VAR1 = 0; VAR2 = Number.NEGATIVE_INFINITY; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = 1; VAR2= Infinity; VAR1 %= VAR2;VAR1",   
              1,     
              eval("VAR1 = 1; VAR2 = Number.POSITIVE_INFINITY; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = -1; VAR2= Infinity; VAR1 %= VAR2;VAR1",  
              -1,    
              eval("VAR1 = -1; VAR2 = Number.POSITIVE_INFINITY; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = -1; VAR2= -Infinity; VAR1 %= VAR2;VAR1", 
              -1,    
              eval("VAR1 = -1; VAR2 = Number.NEGATIVE_INFINITY; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = 1; VAR2= -Infinity; VAR1 %= VAR2;VAR1",  
              1,     
              eval("VAR1 = 1; VAR2 = Number.NEGATIVE_INFINITY; VAR1 %= VAR2; VAR1") );

// if the dividend is a zero and the divisor is finite, the result is the same as the dividend

new TestCase( SECTION,   
              "VAR1 = 0; VAR2= 1; VAR1 %= VAR2; VAR1",   
              0,   
              eval("VAR1 = 0; VAR2 = 1; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = -0; VAR2= 1; VAR1 %= VAR2; VAR1",  
              -0,  
              eval("VAR1 = -0; VAR2 = 1; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = -0; VAR2= -1; VAR1 %= VAR2; VAR1", 
              -0,  
              eval("VAR1 = -0; VAR2 = -1; VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = 0; VAR2= -1; VAR1 %= VAR2; VAR1",  
              0,   
              eval("VAR1 = 0; VAR2 = -1; VAR1 %= VAR2; VAR1") );

// string cases
new TestCase( SECTION,   
              "VAR1 = 1000; VAR2 = '10', VAR1 %= VAR2; VAR1",
              0,      
              eval("VAR1 = 1000; VAR2 = '10', VAR1 %= VAR2; VAR1") );

new TestCase( SECTION,   
              "VAR1 = '1000'; VAR2 = 10, VAR1 %= VAR2; VAR1",
              0,      
              eval("VAR1 = '1000'; VAR2 = 10, VAR1 %= VAR2; VAR1") );
/*
  new TestCase( SECTION,    "VAR1 = 10; VAR2 = '0XFF', VAR1 %= VAR2", 2550,       eval("VAR1 = 10; VAR2 = '0XFF', VAR1 %= VAR2") );
  new TestCase( SECTION,    "VAR1 = '0xFF'; VAR2 = 0xA, VAR1 %= VAR2", 2550,      eval("VAR1 = '0XFF'; VAR2 = 0XA, VAR1 %= VAR2") );

  new TestCase( SECTION,    "VAR1 = '10'; VAR2 = '255', VAR1 %= VAR2", 2550,      eval("VAR1 = '10'; VAR2 = '255', VAR1 %= VAR2") );
  new TestCase( SECTION,    "VAR1 = '10'; VAR2 = '0XFF', VAR1 %= VAR2", 2550,     eval("VAR1 = '10'; VAR2 = '0XFF', VAR1 %= VAR2") );
  new TestCase( SECTION,    "VAR1 = '0xFF'; VAR2 = 0xA, VAR1 %= VAR2", 2550,      eval("VAR1 = '0XFF'; VAR2 = 0XA, VAR1 %= VAR2") );

  // boolean cases
  new TestCase( SECTION,    "VAR1 = true; VAR2 = false; VAR1 %= VAR2",    0,      eval("VAR1 = true; VAR2 = false; VAR1 %= VAR2") );
  new TestCase( SECTION,    "VAR1 = true; VAR2 = true; VAR1 %= VAR2",    1,      eval("VAR1 = true; VAR2 = true; VAR1 %= VAR2") );

  // object cases
  new TestCase( SECTION,    "VAR1 = new Boolean(true); VAR2 = 10; VAR1 %= VAR2;VAR1",    10,      eval("VAR1 = new Boolean(true); VAR2 = 10; VAR1 %= VAR2; VAR1") );
  new TestCase( SECTION,    "VAR1 = new Number(11); VAR2 = 10; VAR1 %= VAR2; VAR1",    110,      eval("VAR1 = new Number(11); VAR2 = 10; VAR1 %= VAR2; VAR1") );
  new TestCase( SECTION,    "VAR1 = new Number(11); VAR2 = new Number(10); VAR1 %= VAR2",    110,      eval("VAR1 = new Number(11); VAR2 = new Number(10); VAR1 %= VAR2") );
  new TestCase( SECTION,    "VAR1 = new String('15'); VAR2 = new String('0xF'); VAR1 %= VAR2",    255,      eval("VAR1 = String('15'); VAR2 = new String('0xF'); VAR1 %= VAR2") );

*/

test();

