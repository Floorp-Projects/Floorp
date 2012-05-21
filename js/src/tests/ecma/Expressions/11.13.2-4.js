/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          11.13.2-4.js
   ECMA Section:       11.13.2 Compound Assignment:+=
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
var SECTION = "11.13.2-4";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " Compound Assignment: +=");

// If either operand is NaN,  result is NaN

new TestCase( SECTION,    "VAR1 = NaN; VAR2=1; VAR1 += VAR2",       Number.NaN, eval("VAR1 = Number.NaN; VAR2=1; VAR1 += VAR2") );
new TestCase( SECTION,    "VAR1 = NaN; VAR2=1; VAR1 += VAR2; VAR1", Number.NaN, eval("VAR1 = Number.NaN; VAR2=1; VAR1 += VAR2; VAR1") );
new TestCase( SECTION,    "VAR1 = NaN; VAR2=0; VAR1 += VAR2",       Number.NaN, eval("VAR1 = Number.NaN; VAR2=0; VAR1 += VAR2") );
new TestCase( SECTION,    "VAR1 = NaN; VAR2=0; VAR1 += VAR2; VAR1", Number.NaN, eval("VAR1 = Number.NaN; VAR2=0; VAR1 += VAR2; VAR1") );
new TestCase( SECTION,    "VAR1 = 0; VAR2=NaN; VAR1 += VAR2",       Number.NaN, eval("VAR1 = 0; VAR2=Number.NaN; VAR1 += VAR2") );
new TestCase( SECTION,    "VAR1 = 0; VAR2=NaN; VAR1 += VAR2; VAR1", Number.NaN, eval("VAR1 = 0; VAR2=Number.NaN; VAR1 += VAR2; VAR1") );

// the sum of two Infinities the same sign is the infinity of that sign
// the sum of two Infinities of opposite sign is NaN

new TestCase( SECTION,    "VAR1 = Infinity; VAR2= Infinity; VAR1 += VAR2; VAR1",   Number.POSITIVE_INFINITY,        eval("VAR1 = Number.POSITIVE_INFINITY; VAR2 = Number.POSITIVE_INFINITY; VAR1 += VAR2; VAR1") );
new TestCase( SECTION,    "VAR1 = Infinity; VAR2= -Infinity; VAR1 += VAR2; VAR1",  Number.NaN,                      eval("VAR1 = Number.POSITIVE_INFINITY; VAR2 = Number.NEGATIVE_INFINITY; VAR1 += VAR2; VAR1") );
new TestCase( SECTION,    "VAR1 =-Infinity; VAR2= Infinity; VAR1 += VAR2; VAR1",   Number.NaN,                      eval("VAR1 = Number.NEGATIVE_INFINITY; VAR2 = Number.POSITIVE_INFINITY; VAR1 += VAR2; VAR1") );
new TestCase( SECTION,    "VAR1 =-Infinity; VAR2=-Infinity; VAR1 += VAR2; VAR1",   Number.NEGATIVE_INFINITY,        eval("VAR1 = Number.NEGATIVE_INFINITY; VAR2 = Number.NEGATIVE_INFINITY; VAR1 += VAR2; VAR1") );

// the sum of an infinity and a finite value is equal to the infinite operand

new TestCase( SECTION,    "VAR1 = 0; VAR2= Infinity; VAR1 += VAR2;VAR1",    Number.POSITIVE_INFINITY,      eval("VAR1 = 0; VAR2 = Number.POSITIVE_INFINITY; VAR1 += VAR2; VAR1") );
new TestCase( SECTION,    "VAR1 = -0; VAR2= Infinity; VAR1 += VAR2;VAR1",   Number.POSITIVE_INFINITY,      eval("VAR1 = -0; VAR2 = Number.POSITIVE_INFINITY; VAR1 += VAR2; VAR1") );
new TestCase( SECTION,    "VAR1 = -0; VAR2= -Infinity; VAR1 += VAR2;VAR1",  Number.NEGATIVE_INFINITY,      eval("VAR1 = -0; VAR2 = Number.NEGATIVE_INFINITY; VAR1 += VAR2; VAR1") );
new TestCase( SECTION,    "VAR1 = 0; VAR2= -Infinity; VAR1 += VAR2;VAR1",   Number.NEGATIVE_INFINITY,      eval("VAR1 = 0; VAR2 = Number.NEGATIVE_INFINITY; VAR1 += VAR2; VAR1") );

// the sum of two negative zeros is -0. the sum of two positive zeros, or of two zeros of opposite sign, is +0

new TestCase( SECTION,    "VAR1 = 0; VAR2= 0; VAR1 += VAR2",    0,      eval("VAR1 = 0; VAR2 = 0; VAR1 += VAR2; VAR1") );
new TestCase( SECTION,    "VAR1 = 0; VAR2= -0; VAR1 += VAR2",   0,      eval("VAR1 = 0; VAR2 = -0; VAR1 += VAR2; VAR1") );
new TestCase( SECTION,    "VAR1 = -0; VAR2= 0; VAR1 += VAR2",   0,      eval("VAR1 = -0; VAR2 = 0; VAR1 += VAR2; VAR1") );
new TestCase( SECTION,    "VAR1 = -0; VAR2= -0; VAR1 += VAR2",  -0,      eval("VAR1 = -0; VAR2 = -0; VAR1 += VAR2; VAR1") );

//  the sum of a zero and a nonzero finite value is eqal to the nonzero operand

new TestCase( SECTION,    "VAR1 = 0; VAR2= 1; VAR2 += VAR1; VAR2",    1,      eval("VAR1 = 0; VAR2 = 1; VAR2 += VAR1; VAR2") );
new TestCase( SECTION,    "VAR1 = -0; VAR2= 1; VAR2 += VAR1; VAR2",   1,      eval("VAR1 = -0; VAR2 = 1; VAR2 += VAR1; VAR2") );
new TestCase( SECTION,    "VAR1 = -0; VAR2= -1; VAR2 += VAR1; VAR2",  -1,      eval("VAR1 = -0; VAR2 = -1; VAR2 += VAR1; VAR2") );
new TestCase( SECTION,    "VAR1 = 0; VAR2= -1; VAR2 += VAR1; VAR2",   -1,      eval("VAR1 = 0; VAR2 = -1; VAR2 += VAR1; VAR2") );

// the sum of a zero and a nozero finite value is equal to the nonzero operand.
new TestCase( SECTION,    "VAR1 = 0; VAR2=1; VAR1 += VAR2",         1,          eval("VAR1 = 0; VAR2=1; VAR1 += VAR2") );
new TestCase( SECTION,    "VAR1 = 0; VAR2=1; VAR1 += VAR2;VAR1",    1,          eval("VAR1 = 0; VAR2=1; VAR1 += VAR2;VAR1") );

// the sum of two nonzero finite values of the same magnitude and opposite sign is +0
new TestCase( SECTION,    "VAR1 = Number.MAX_VALUE; VAR2= -Number.MAX_VALUE; VAR1 += VAR2; VAR1",    0,  eval("VAR1 = Number.MAX_VALUE; VAR2= -Number.MAX_VALUE; VAR1 += VAR2; VAR1") );
new TestCase( SECTION,    "VAR1 = Number.MIN_VALUE; VAR2= -Number.MIN_VALUE; VAR1 += VAR2; VAR1",   0,  eval("VAR1 = Number.MIN_VALUE; VAR2= -Number.MIN_VALUE; VAR1 += VAR2; VAR1") );

/*
  new TestCase( SECTION,    "VAR1 = 10; VAR2 = '0XFF', VAR1 += VAR2", 2550,       eval("VAR1 = 10; VAR2 = '0XFF', VAR1 += VAR2") );
  new TestCase( SECTION,    "VAR1 = '0xFF'; VAR2 = 0xA, VAR1 += VAR2", 2550,      eval("VAR1 = '0XFF'; VAR2 = 0XA, VAR1 += VAR2") );

  new TestCase( SECTION,    "VAR1 = '10'; VAR2 = '255', VAR1 += VAR2", 2550,      eval("VAR1 = '10'; VAR2 = '255', VAR1 += VAR2") );
  new TestCase( SECTION,    "VAR1 = '10'; VAR2 = '0XFF', VAR1 += VAR2", 2550,     eval("VAR1 = '10'; VAR2 = '0XFF', VAR1 += VAR2") );
  new TestCase( SECTION,    "VAR1 = '0xFF'; VAR2 = 0xA, VAR1 += VAR2", 2550,      eval("VAR1 = '0XFF'; VAR2 = 0XA, VAR1 += VAR2") );

  // boolean cases
  new TestCase( SECTION,    "VAR1 = true; VAR2 = false; VAR1 += VAR2",    0,      eval("VAR1 = true; VAR2 = false; VAR1 += VAR2") );
  new TestCase( SECTION,    "VAR1 = true; VAR2 = true; VAR1 += VAR2",    1,      eval("VAR1 = true; VAR2 = true; VAR1 += VAR2") );

  // object cases
  new TestCase( SECTION,    "VAR1 = new Boolean(true); VAR2 = 10; VAR1 += VAR2;VAR1",    10,      eval("VAR1 = new Boolean(true); VAR2 = 10; VAR1 += VAR2; VAR1") );
  new TestCase( SECTION,    "VAR1 = new Number(11); VAR2 = 10; VAR1 += VAR2; VAR1",    110,      eval("VAR1 = new Number(11); VAR2 = 10; VAR1 += VAR2; VAR1") );
  new TestCase( SECTION,    "VAR1 = new Number(11); VAR2 = new Number(10); VAR1 += VAR2",    110,      eval("VAR1 = new Number(11); VAR2 = new Number(10); VAR1 += VAR2") );
  new TestCase( SECTION,    "VAR1 = new String('15'); VAR2 = new String('0xF'); VAR1 += VAR2",    255,      eval("VAR1 = String('15'); VAR2 = new String('0xF'); VAR1 += VAR2") );

*/

test();
