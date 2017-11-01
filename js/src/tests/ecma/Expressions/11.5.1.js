/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          11.5.1.js
   ECMA Section:       11.5.1 Applying the * operator
   Description:

   11.5.1 Applying the * operator

   The * operator performs multiplication, producing the product of its
   operands. Multiplication is commutative. Multiplication is not always
   associative in ECMAScript, because of finite precision.

   The result of a floating-point multiplication is governed by the rules
   of IEEE 754 double-precision arithmetic:

   If either operand is NaN, the result is NaN.
   The sign of the result is positive if both operands have the same sign,
   negative if the operands have different signs.
   Multiplication of an infinity by a zero results in NaN.
   Multiplication of an infinity by an infinity results in an infinity.
   The sign is determined by the rule already stated above.
   Multiplication of an infinity by a finite non-zero value results in a
   signed infinity. The sign is determined by the rule already stated above.
   In the remaining cases, where neither an infinity or NaN is involved, the
   product is computed and rounded to the nearest representable value using IEEE
   754 round-to-nearest mode. If the magnitude is too large to represent,
   the result is then an infinity of appropriate sign. If the magnitude is
   oo small to represent, the result is then a zero
   of appropriate sign. The ECMAScript language requires support of gradual
   underflow as defined by IEEE 754.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/
var SECTION = "11.5.1";

writeHeaderToLog( SECTION + " Applying the * operator");

new TestCase( "Number.NaN * Number.NaN",    Number.NaN,     Number.NaN * Number.NaN );
new TestCase( "Number.NaN * 1",             Number.NaN,     Number.NaN * 1 );
new TestCase( "1 * Number.NaN",             Number.NaN,     1 * Number.NaN );

new TestCase( "Number.POSITIVE_INFINITY * 0",   Number.NaN, Number.POSITIVE_INFINITY * 0 );
new TestCase( "Number.NEGATIVE_INFINITY * 0",   Number.NaN, Number.NEGATIVE_INFINITY * 0 );
new TestCase( "0 * Number.POSITIVE_INFINITY",   Number.NaN, 0 * Number.POSITIVE_INFINITY );
new TestCase( "0 * Number.NEGATIVE_INFINITY",   Number.NaN, 0 * Number.NEGATIVE_INFINITY );

new TestCase( "-0 * Number.POSITIVE_INFINITY",  Number.NaN,   -0 * Number.POSITIVE_INFINITY );
new TestCase( "-0 * Number.NEGATIVE_INFINITY",  Number.NaN,   -0 * Number.NEGATIVE_INFINITY );
new TestCase( "Number.POSITIVE_INFINITY * -0",  Number.NaN,   Number.POSITIVE_INFINITY * -0 );
new TestCase( "Number.NEGATIVE_INFINITY * -0",  Number.NaN,   Number.NEGATIVE_INFINITY * -0 );

new TestCase( "0 * -0",                         -0,         0 * -0 );
new TestCase( "-0 * 0",                         -0,         -0 * 0 );
new TestCase( "-0 * -0",                        0,          -0 * -0 );
new TestCase( "0 * 0",                          0,          0 * 0 );

new TestCase( "Number.NEGATIVE_INFINITY * Number.NEGATIVE_INFINITY",    Number.POSITIVE_INFINITY,   Number.NEGATIVE_INFINITY * Number.NEGATIVE_INFINITY );
new TestCase( "Number.POSITIVE_INFINITY * Number.NEGATIVE_INFINITY",    Number.NEGATIVE_INFINITY,   Number.POSITIVE_INFINITY * Number.NEGATIVE_INFINITY );
new TestCase( "Number.NEGATIVE_INFINITY * Number.POSITIVE_INFINITY",    Number.NEGATIVE_INFINITY,   Number.NEGATIVE_INFINITY * Number.POSITIVE_INFINITY );
new TestCase( "Number.POSITIVE_INFINITY * Number.POSITIVE_INFINITY",    Number.POSITIVE_INFINITY,   Number.POSITIVE_INFINITY * Number.POSITIVE_INFINITY );

new TestCase( "Number.NEGATIVE_INFINITY * 1 ",                          Number.NEGATIVE_INFINITY,   Number.NEGATIVE_INFINITY * 1 );
new TestCase( "Number.NEGATIVE_INFINITY * -1 ",                         Number.POSITIVE_INFINITY,   Number.NEGATIVE_INFINITY * -1 );
new TestCase( "1 * Number.NEGATIVE_INFINITY",                           Number.NEGATIVE_INFINITY,   1 * Number.NEGATIVE_INFINITY );
new TestCase( "-1 * Number.NEGATIVE_INFINITY",                          Number.POSITIVE_INFINITY,   -1 * Number.NEGATIVE_INFINITY );

new TestCase( "Number.POSITIVE_INFINITY * 1 ",                          Number.POSITIVE_INFINITY,   Number.POSITIVE_INFINITY * 1 );
new TestCase( "Number.POSITIVE_INFINITY * -1 ",                         Number.NEGATIVE_INFINITY,   Number.POSITIVE_INFINITY * -1 );
new TestCase( "1 * Number.POSITIVE_INFINITY",                           Number.POSITIVE_INFINITY,   1 * Number.POSITIVE_INFINITY );
new TestCase( "-1 * Number.POSITIVE_INFINITY",                          Number.NEGATIVE_INFINITY,   -1 * Number.POSITIVE_INFINITY );

test();

