/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.1.2.3-2.js
   ECMA Section:       15.1.2.3 Function properties of the global object:
   parseFloat( string )

   Description:        The parseFloat function produces a number value dictated
   by the interpretation of the contents of the string
   argument defined as a decimal literal.

   When the parseFloat function is called, the following
   steps are taken:

   1.  Call ToString( string ).
   2.  Remove leading whitespace Result(1).
   3.  If neither Result(2) nor any prefix of Result(2)
   satisfies the syntax of a StrDecimalLiteral,
   return NaN.
   4.  Compute the longest prefix of Result(2) which might
   be Resusult(2) itself, that satisfies the syntax of
   a StrDecimalLiteral
   5.  Return the number value for the MV of Result(4).

   Note that parseFloate may interpret only a leading
   portion of the string as a number value; it ignores any
   characters that cannot be interpreted as part of the
   notation of a decimal literal, and no indication is given
   that such characters were ignored.

   StrDecimalLiteral::
   Infinity
   DecimalDigits.DecimalDigits opt ExponentPart opt
   .DecimalDigits ExponentPart opt
   DecimalDigits ExponentPart opt

   Author:             christine@netscape.com
   Date:               28 october 1997

*/
var SECTION = "15.1.2.3-2";

new TestCase( "parseFloat(true)",      Number.NaN,     parseFloat(true) );
new TestCase( "parseFloat(false)",     Number.NaN,     parseFloat(false) );
new TestCase( "parseFloat('string')",  Number.NaN,     parseFloat("string") );

new TestCase( "parseFloat('     Infinity')",      Number.POSITIVE_INFINITY,    parseFloat("Infinity") );
//     new TestCase( "parseFloat(Infinity)",      Number.POSITIVE_INFINITY,    parseFloat(Infinity) );

new TestCase( "parseFloat('          0')",          0,          parseFloat("          0") );
new TestCase( "parseFloat('          -0')",         -0,         parseFloat("          -0") );
new TestCase( "parseFloat('          +0')",          0,         parseFloat("          +0") );

new TestCase( "parseFloat('          1')",          1,          parseFloat("          1") );
new TestCase( "parseFloat('          -1')",         -1,         parseFloat("          -1") );
new TestCase( "parseFloat('          +1')",          1,         parseFloat("          +1") );

new TestCase( "parseFloat('          2')",          2,          parseFloat("          2") );
new TestCase( "parseFloat('          -2')",         -2,         parseFloat("          -2") );
new TestCase( "parseFloat('          +2')",          2,         parseFloat("          +2") );

new TestCase( "parseFloat('          3')",          3,          parseFloat("          3") );
new TestCase( "parseFloat('          -3')",         -3,         parseFloat("          -3") );
new TestCase( "parseFloat('          +3')",          3,         parseFloat("          +3") );

new TestCase( "parseFloat('          4')",          4,          parseFloat("          4") );
new TestCase( "parseFloat('          -4')",         -4,         parseFloat("          -4") );
new TestCase( "parseFloat('          +4')",          4,         parseFloat("          +4") );

new TestCase( "parseFloat('          5')",          5,          parseFloat("          5") );
new TestCase( "parseFloat('          -5')",         -5,         parseFloat("          -5") );
new TestCase( "parseFloat('          +5')",          5,         parseFloat("          +5") );

new TestCase( "parseFloat('          6')",          6,          parseFloat("          6") );
new TestCase( "parseFloat('          -6')",         -6,         parseFloat("          -6") );
new TestCase( "parseFloat('          +6')",          6,         parseFloat("          +6") );

new TestCase( "parseFloat('          7')",          7,          parseFloat("          7") );
new TestCase( "parseFloat('          -7')",         -7,         parseFloat("          -7") );
new TestCase( "parseFloat('          +7')",          7,         parseFloat("          +7") );

new TestCase( "parseFloat('          8')",          8,          parseFloat("          8") );
new TestCase( "parseFloat('          -8')",         -8,         parseFloat("          -8") );
new TestCase( "parseFloat('          +8')",          8,         parseFloat("          +8") );

new TestCase( "parseFloat('          9')",          9,          parseFloat("          9") );
new TestCase( "parseFloat('          -9')",         -9,         parseFloat("          -9") );
new TestCase( "parseFloat('          +9')",          9,         parseFloat("          +9") );

new TestCase( "parseFloat('          3.14159')",    3.14159,    parseFloat("          3.14159") );
new TestCase( "parseFloat('          -3.14159')",   -3.14159,   parseFloat("          -3.14159") );
new TestCase( "parseFloat('          +3.14159')",   3.14159,    parseFloat("          +3.14159") );

new TestCase( "parseFloat('          3.')",         3,          parseFloat("          3.") );
new TestCase( "parseFloat('          -3.')",        -3,         parseFloat("          -3.") );
new TestCase( "parseFloat('          +3.')",        3,          parseFloat("          +3.") );

new TestCase( "parseFloat('          3.e1')",       30,         parseFloat("          3.e1") );
new TestCase( "parseFloat('          -3.e1')",      -30,        parseFloat("          -3.e1") );
new TestCase( "parseFloat('          +3.e1')",      30,         parseFloat("          +3.e1") );

new TestCase( "parseFloat('          3.e+1')",       30,         parseFloat("          3.e+1") );
new TestCase( "parseFloat('          -3.e+1')",      -30,        parseFloat("          -3.e+1") );
new TestCase( "parseFloat('          +3.e+1')",      30,         parseFloat("          +3.e+1") );

new TestCase( "parseFloat('          3.e-1')",       .30,         parseFloat("          3.e-1") );
new TestCase( "parseFloat('          -3.e-1')",      -.30,        parseFloat("          -3.e-1") );
new TestCase( "parseFloat('          +3.e-1')",      .30,         parseFloat("          +3.e-1") );

// StrDecimalLiteral:::  .DecimalDigits ExponentPart opt

new TestCase( "parseFloat('          .00001')",     0.00001,    parseFloat("          .00001") );
new TestCase( "parseFloat('          +.00001')",    0.00001,    parseFloat("          +.00001") );
new TestCase( "parseFloat('          -0.0001')",    -0.00001,   parseFloat("          -.00001") );

new TestCase( "parseFloat('          .01e2')",      1,          parseFloat("          .01e2") );
new TestCase( "parseFloat('          +.01e2')",     1,          parseFloat("          +.01e2") );
new TestCase( "parseFloat('          -.01e2')",     -1,         parseFloat("          -.01e2") );

new TestCase( "parseFloat('          .01e+2')",      1,         parseFloat("          .01e+2") );
new TestCase( "parseFloat('          +.01e+2')",     1,         parseFloat("          +.01e+2") );
new TestCase( "parseFloat('          -.01e+2')",     -1,        parseFloat("          -.01e+2") );

new TestCase( "parseFloat('          .01e-2')",      0.0001,    parseFloat("          .01e-2") );
new TestCase( "parseFloat('          +.01e-2')",     0.0001,    parseFloat("          +.01e-2") );
new TestCase( "parseFloat('          -.01e-2')",     -0.0001,   parseFloat("          -.01e-2") );

//  StrDecimalLiteral:::    DecimalDigits ExponentPart opt

new TestCase( "parseFloat('          1234e5')",     123400000,  parseFloat("          1234e5") );
new TestCase( "parseFloat('          +1234e5')",    123400000,  parseFloat("          +1234e5") );
new TestCase( "parseFloat('          -1234e5')",    -123400000, parseFloat("          -1234e5") );

new TestCase( "parseFloat('          1234e+5')",    123400000,  parseFloat("          1234e+5") );
new TestCase( "parseFloat('          +1234e+5')",   123400000,  parseFloat("          +1234e+5") );
new TestCase( "parseFloat('          -1234e+5')",   -123400000, parseFloat("          -1234e+5") );

new TestCase( "parseFloat('          1234e-5')",     0.01234,  parseFloat("          1234e-5") );
new TestCase( "parseFloat('          +1234e-5')",    0.01234,  parseFloat("          +1234e-5") );
new TestCase( "parseFloat('          -1234e-5')",    -0.01234, parseFloat("          -1234e-5") );


new TestCase( "parseFloat('          .01E2')",      1,          parseFloat("          .01E2") );
new TestCase( "parseFloat('          +.01E2')",     1,          parseFloat("          +.01E2") );
new TestCase( "parseFloat('          -.01E2')",     -1,         parseFloat("          -.01E2") );

new TestCase( "parseFloat('          .01E+2')",      1,         parseFloat("          .01E+2") );
new TestCase( "parseFloat('          +.01E+2')",     1,         parseFloat("          +.01E+2") );
new TestCase( "parseFloat('          -.01E+2')",     -1,        parseFloat("          -.01E+2") );

new TestCase( "parseFloat('          .01E-2')",      0.0001,    parseFloat("          .01E-2") );
new TestCase( "parseFloat('          +.01E-2')",     0.0001,    parseFloat("          +.01E-2") );
new TestCase( "parseFloat('          -.01E-2')",     -0.0001,   parseFloat("          -.01E-2") );

//  StrDecimalLiteral:::    DecimalDigits ExponentPart opt
new TestCase( "parseFloat('          1234E5')",     123400000,  parseFloat("          1234E5") );
new TestCase( "parseFloat('          +1234E5')",    123400000,  parseFloat("          +1234E5") );
new TestCase( "parseFloat('          -1234E5')",    -123400000, parseFloat("          -1234E5") );

new TestCase( "parseFloat('          1234E+5')",    123400000,  parseFloat("          1234E+5") );
new TestCase( "parseFloat('          +1234E+5')",   123400000,  parseFloat("          +1234E+5") );
new TestCase( "parseFloat('          -1234E+5')",   -123400000, parseFloat("          -1234E+5") );

new TestCase( "parseFloat('          1234E-5')",     0.01234,  parseFloat("          1234E-5") );
new TestCase( "parseFloat('          +1234E-5')",    0.01234,  parseFloat("          +1234E-5") );
new TestCase( "parseFloat('          -1234E-5')",    -0.01234, parseFloat("          -1234E-5") );


// hex cases should all return NaN

new TestCase( "parseFloat('          0x0')",        0,         parseFloat("          0x0"));
new TestCase( "parseFloat('          0x1')",        0,         parseFloat("          0x1"));
new TestCase( "parseFloat('          0x2')",        0,         parseFloat("          0x2"));
new TestCase( "parseFloat('          0x3')",        0,         parseFloat("          0x3"));
new TestCase( "parseFloat('          0x4')",        0,         parseFloat("          0x4"));
new TestCase( "parseFloat('          0x5')",        0,         parseFloat("          0x5"));
new TestCase( "parseFloat('          0x6')",        0,         parseFloat("          0x6"));
new TestCase( "parseFloat('          0x7')",        0,         parseFloat("          0x7"));
new TestCase( "parseFloat('          0x8')",        0,         parseFloat("          0x8"));
new TestCase( "parseFloat('          0x9')",        0,         parseFloat("          0x9"));
new TestCase( "parseFloat('          0xa')",        0,         parseFloat("          0xa"));
new TestCase( "parseFloat('          0xb')",        0,         parseFloat("          0xb"));
new TestCase( "parseFloat('          0xc')",        0,         parseFloat("          0xc"));
new TestCase( "parseFloat('          0xd')",        0,         parseFloat("          0xd"));
new TestCase( "parseFloat('          0xe')",        0,         parseFloat("          0xe"));
new TestCase( "parseFloat('          0xf')",        0,         parseFloat("          0xf"));
new TestCase( "parseFloat('          0xA')",        0,         parseFloat("          0xA"));
new TestCase( "parseFloat('          0xB')",        0,         parseFloat("          0xB"));
new TestCase( "parseFloat('          0xC')",        0,         parseFloat("          0xC"));
new TestCase( "parseFloat('          0xD')",        0,         parseFloat("          0xD"));
new TestCase( "parseFloat('          0xE')",        0,         parseFloat("          0xE"));
new TestCase( "parseFloat('          0xF')",        0,         parseFloat("          0xF"));

new TestCase( "parseFloat('          0X0')",        0,         parseFloat("          0X0"));
new TestCase( "parseFloat('          0X1')",        0,         parseFloat("          0X1"));
new TestCase( "parseFloat('          0X2')",        0,         parseFloat("          0X2"));
new TestCase( "parseFloat('          0X3')",        0,         parseFloat("          0X3"));
new TestCase( "parseFloat('          0X4')",        0,         parseFloat("          0X4"));
new TestCase( "parseFloat('          0X5')",        0,         parseFloat("          0X5"));
new TestCase( "parseFloat('          0X6')",        0,         parseFloat("          0X6"));
new TestCase( "parseFloat('          0X7')",        0,         parseFloat("          0X7"));
new TestCase( "parseFloat('          0X8')",        0,         parseFloat("          0X8"));
new TestCase( "parseFloat('          0X9')",        0,         parseFloat("          0X9"));
new TestCase( "parseFloat('          0Xa')",        0,         parseFloat("          0Xa"));
new TestCase( "parseFloat('          0Xb')",        0,         parseFloat("          0Xb"));
new TestCase( "parseFloat('          0Xc')",        0,         parseFloat("          0Xc"));
new TestCase( "parseFloat('          0Xd')",        0,         parseFloat("          0Xd"));
new TestCase( "parseFloat('          0Xe')",        0,         parseFloat("          0Xe"));
new TestCase( "parseFloat('          0Xf')",        0,         parseFloat("          0Xf"));
new TestCase( "parseFloat('          0XA')",        0,         parseFloat("          0XA"));
new TestCase( "parseFloat('          0XB')",        0,         parseFloat("          0XB"));
new TestCase( "parseFloat('          0XC')",        0,         parseFloat("          0XC"));
new TestCase( "parseFloat('          0XD')",        0,         parseFloat("          0XD"));
new TestCase( "parseFloat('          0XE')",        0,         parseFloat("          0XE"));
new TestCase( "parseFloat('          0XF')",        0,         parseFloat("          0XF"));

// A StringNumericLiteral may not use octal notation

new TestCase( "parseFloat('          00')",        0,         parseFloat("          00"));
new TestCase( "parseFloat('          01')",        1,         parseFloat("          01"));
new TestCase( "parseFloat('          02')",        2,         parseFloat("          02"));
new TestCase( "parseFloat('          03')",        3,         parseFloat("          03"));
new TestCase( "parseFloat('          04')",        4,         parseFloat("          04"));
new TestCase( "parseFloat('          05')",        5,         parseFloat("          05"));
new TestCase( "parseFloat('          06')",        6,         parseFloat("          06"));
new TestCase( "parseFloat('          07')",        7,         parseFloat("          07"));
new TestCase( "parseFloat('          010')",       10,        parseFloat("          010"));
new TestCase( "parseFloat('          011')",       11,        parseFloat("          011"));

// A StringNumericLIteral may have any number of leading 0 digits

new TestCase( "parseFloat('          001')",        1,         parseFloat("          001"));
new TestCase( "parseFloat('          0001')",       1,         parseFloat("          0001"));

// A StringNumericLIteral may have any number of leading 0 digits

new TestCase( "parseFloat(001)",        1,         parseFloat(001));
new TestCase( "parseFloat(0001)",       1,         parseFloat(0001));

// make sure it'          s reflexive
new TestCase( "parseFloat( '                    '          +Math.PI+'          ')",      Math.PI,        parseFloat( '                    '          +Math.PI+'          '));
new TestCase( "parseFloat( '                    '          +Math.LN2+'          ')",     Math.LN2,       parseFloat( '                    '          +Math.LN2+'          '));
new TestCase( "parseFloat( '                    '          +Math.LN10+'          ')",    Math.LN10,      parseFloat( '                    '          +Math.LN10+'          '));
new TestCase( "parseFloat( '                    '          +Math.LOG2E+'          ')",   Math.LOG2E,     parseFloat( '                    '          +Math.LOG2E+'          '));
new TestCase( "parseFloat( '                    '          +Math.LOG10E+'          ')",  Math.LOG10E,    parseFloat( '                    '          +Math.LOG10E+'          '));
new TestCase( "parseFloat( '                    '          +Math.SQRT2+'          ')",   Math.SQRT2,     parseFloat( '                    '          +Math.SQRT2+'          '));
new TestCase( "parseFloat( '                    '          +Math.SQRT1_2+'          ')", Math.SQRT1_2,   parseFloat( '                    '          +Math.SQRT1_2+'          '));

test();
