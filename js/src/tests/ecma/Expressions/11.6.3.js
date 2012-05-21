/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          11.6.3.js
   ECMA Section:       11.6.3 Applying the additive operators
   (+, -) to numbers
   Description:
   The + operator performs addition when applied to two operands of numeric
   type, producing the sum of the operands. The - operator performs
   subtraction, producing the difference of two numeric operands.

   Addition is a commutative operation, but not always associative.

   The result of an addition is determined using the rules of IEEE 754
   double-precision arithmetic:

   If either operand is NaN, the result is NaN.
   The sum of two infinities of opposite sign is NaN.
   The sum of two infinities of the same sign is the infinity of that sign.
   The sum of an infinity and a finite value is equal to the infinite operand.
   The sum of two negative zeros is 0. The sum of two positive zeros, or of
   two zeros of opposite sign, is +0.
   The sum of a zero and a nonzero finite value is equal to the nonzero
   operand.
   The sum of two nonzero finite values of the same magnitude and opposite
   sign is +0.
   In the remaining cases, where neither an infinity, nor a zero, nor NaN is
   involved, and the operands have the same sign or have different
   magnitudes, the sum is computed and rounded to the nearest
   representable value using IEEE 754 round-to-nearest mode. If the
   magnitude is too large to represent, the operation overflows and
   the result is then an infinity of appropriate sign. The ECMAScript
   language requires support of gradual underflow as defined by IEEE 754.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.6.3";
var VERSION = "ECMA_1";
startTest();

writeHeaderToLog( SECTION + " Applying the additive operators (+,-) to numbers");

new TestCase( SECTION,    "Number.NaN + 1",     Number.NaN,     Number.NaN + 1 );
new TestCase( SECTION,    "1 + Number.NaN",     Number.NaN,     1 + Number.NaN );

new TestCase( SECTION,    "Number.NaN - 1",     Number.NaN,     Number.NaN - 1 );
new TestCase( SECTION,    "1 - Number.NaN",     Number.NaN,     1 - Number.NaN );

new TestCase( SECTION,  "Number.POSITIVE_INFINITY + Number.POSITIVE_INFINITY",  Number.POSITIVE_INFINITY,   Number.POSITIVE_INFINITY + Number.POSITIVE_INFINITY);
new TestCase( SECTION,  "Number.NEGATIVE_INFINITY + Number.NEGATIVE_INFINITY",  Number.NEGATIVE_INFINITY,   Number.NEGATIVE_INFINITY + Number.NEGATIVE_INFINITY);

new TestCase( SECTION,  "Number.POSITIVE_INFINITY + Number.NEGATIVE_INFINITY",  Number.NaN,     Number.POSITIVE_INFINITY + Number.NEGATIVE_INFINITY);
new TestCase( SECTION,  "Number.NEGATIVE_INFINITY + Number.POSITIVE_INFINITY",  Number.NaN,     Number.NEGATIVE_INFINITY + Number.POSITIVE_INFINITY);

new TestCase( SECTION,  "Number.POSITIVE_INFINITY - Number.POSITIVE_INFINITY",  Number.NaN,   Number.POSITIVE_INFINITY - Number.POSITIVE_INFINITY);
new TestCase( SECTION,  "Number.NEGATIVE_INFINITY - Number.NEGATIVE_INFINITY",  Number.NaN,   Number.NEGATIVE_INFINITY - Number.NEGATIVE_INFINITY);

new TestCase( SECTION,  "Number.POSITIVE_INFINITY - Number.NEGATIVE_INFINITY",  Number.POSITIVE_INFINITY,   Number.POSITIVE_INFINITY - Number.NEGATIVE_INFINITY);
new TestCase( SECTION,  "Number.NEGATIVE_INFINITY - Number.POSITIVE_INFINITY",  Number.NEGATIVE_INFINITY,   Number.NEGATIVE_INFINITY - Number.POSITIVE_INFINITY);

new TestCase( SECTION,  "-0 + -0",      -0,     -0 + -0 );
new TestCase( SECTION,  "-0 - 0",       -0,     -0 - 0 );

new TestCase( SECTION,  "0 + 0",        0,      0 + 0 );
new TestCase( SECTION,  "0 + -0",       0,      0 + -0 );
new TestCase( SECTION,  "0 - -0",       0,      0 - -0 );
new TestCase( SECTION,  "0 - 0",        0,      0 - 0 );
new TestCase( SECTION,  "-0 - -0",      0,     -0 - -0 );
new TestCase( SECTION,  "-0 + 0",       0,     -0 + 0 );

new TestCase( SECTION,  "Number.MAX_VALUE - Number.MAX_VALUE",      0,  Number.MAX_VALUE - Number.MAX_VALUE );
new TestCase( SECTION,  "1/Number.MAX_VALUE - 1/Number.MAX_VALUE",  0,  1/Number.MAX_VALUE - 1/Number.MAX_VALUE );

new TestCase( SECTION,  "Number.MIN_VALUE - Number.MIN_VALUE",      0,  Number.MIN_VALUE - Number.MIN_VALUE );

test();
