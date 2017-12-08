/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          11.4.7-01.js
   ECMA Section:       11.4.7 Unary - Operator
   Description:        convert operand to Number type and change sign
   Author:             christine@netscape.com
   Date:               7 july 1997
*/

var SECTION = "11.4.7";
var BUGNUMBER="77391";

printBugNumber(BUGNUMBER);

writeHeaderToLog( SECTION + " Unary + operator");

new TestCase( "-('')",           -0,      -("") );
new TestCase( "-(' ')",          -0,      -(" ") );
new TestCase( "-(\\t)",          -0,      -("\t") );
new TestCase( "-(\\n)",          -0,      -("\n") );
new TestCase( "-(\\r)",          -0,      -("\r") );
new TestCase( "-(\\f)",          -0,      -("\f") );

new TestCase( "-(String.fromCharCode(0x0009)",   -0,  -(String.fromCharCode(0x0009)) );
new TestCase( "-(String.fromCharCode(0x0020)",   -0,  -(String.fromCharCode(0x0020)) );
new TestCase( "-(String.fromCharCode(0x000C)",   -0,  -(String.fromCharCode(0x000C)) );
new TestCase( "-(String.fromCharCode(0x000B)",   -0,  -(String.fromCharCode(0x000B)) );
new TestCase( "-(String.fromCharCode(0x000D)",   -0,  -(String.fromCharCode(0x000D)) );
new TestCase( "-(String.fromCharCode(0x000A)",   -0,  -(String.fromCharCode(0x000A)) );

//  a StringNumericLiteral may be preceeded or followed by whitespace and/or
//  line terminators

new TestCase( "-( '   ' +  999 )",        -999,    -( '   '+999) );
new TestCase( "-( '\\n'  + 999 )",       -999,    -( '\n' +999) );
new TestCase( "-( '\\r'  + 999 )",       -999,    -( '\r' +999) );
new TestCase( "-( '\\t'  + 999 )",       -999,    -( '\t' +999) );
new TestCase( "-( '\\f'  + 999 )",       -999,    -( '\f' +999) );

new TestCase( "-( 999 + '   ' )",        -999,    -( 999+'   ') );
new TestCase( "-( 999 + '\\n' )",        -999,    -( 999+'\n' ) );
new TestCase( "-( 999 + '\\r' )",        -999,    -( 999+'\r' ) );
new TestCase( "-( 999 + '\\t' )",        -999,    -( 999+'\t' ) );
new TestCase( "-( 999 + '\\f' )",        -999,    -( 999+'\f' ) );

new TestCase( "-( '\\n'  + 999 + '\\n' )",         -999,    -( '\n' +999+'\n' ) );
new TestCase( "-( '\\r'  + 999 + '\\r' )",         -999,    -( '\r' +999+'\r' ) );
new TestCase( "-( '\\t'  + 999 + '\\t' )",         -999,    -( '\t' +999+'\t' ) );
new TestCase( "-( '\\f'  + 999 + '\\f' )",         -999,    -( '\f' +999+'\f' ) );

new TestCase( "-( '   ' +  '999' )",     -999,    -( '   '+'999') );
new TestCase( "-( '\\n'  + '999' )",       -999,    -( '\n' +'999') );
new TestCase( "-( '\\r'  + '999' )",       -999,    -( '\r' +'999') );
new TestCase( "-( '\\t'  + '999' )",       -999,    -( '\t' +'999') );
new TestCase( "-( '\\f'  + '999' )",       -999,    -( '\f' +'999') );

new TestCase( "-( '999' + '   ' )",        -999,    -( '999'+'   ') );
new TestCase( "-( '999' + '\\n' )",        -999,    -( '999'+'\n' ) );
new TestCase( "-( '999' + '\\r' )",        -999,    -( '999'+'\r' ) );
new TestCase( "-( '999' + '\\t' )",        -999,    -( '999'+'\t' ) );
new TestCase( "-( '999' + '\\f' )",        -999,    -( '999'+'\f' ) );

new TestCase( "-( '\\n'  + '999' + '\\n' )",         -999,    -( '\n' +'999'+'\n' ) );
new TestCase( "-( '\\r'  + '999' + '\\r' )",         -999,    -( '\r' +'999'+'\r' ) );
new TestCase( "-( '\\t'  + '999' + '\\t' )",         -999,    -( '\t' +'999'+'\t' ) );
new TestCase( "-( '\\f'  + '999' + '\\f' )",         -999,    -( '\f' +'999'+'\f' ) );

new TestCase( "-( String.fromCharCode(0x0009) +  '99' )",    -99,     -( String.fromCharCode(0x0009) +  '99' ) );
new TestCase( "-( String.fromCharCode(0x0020) +  '99' )",    -99,     -( String.fromCharCode(0x0020) +  '99' ) );
new TestCase( "-( String.fromCharCode(0x000C) +  '99' )",    -99,     -( String.fromCharCode(0x000C) +  '99' ) );
new TestCase( "-( String.fromCharCode(0x000B) +  '99' )",    -99,     -( String.fromCharCode(0x000B) +  '99' ) );
new TestCase( "-( String.fromCharCode(0x000D) +  '99' )",    -99,     -( String.fromCharCode(0x000D) +  '99' ) );
new TestCase( "-( String.fromCharCode(0x000A) +  '99' )",    -99,     -( String.fromCharCode(0x000A) +  '99' ) );

new TestCase( "-( String.fromCharCode(0x0009) +  '99' + String.fromCharCode(0x0009)",    -99,     -( String.fromCharCode(0x0009) +  '99' + String.fromCharCode(0x0009)) );
new TestCase( "-( String.fromCharCode(0x0020) +  '99' + String.fromCharCode(0x0020)",    -99,     -( String.fromCharCode(0x0009) +  '99' + String.fromCharCode(0x0020)) );
new TestCase( "-( String.fromCharCode(0x000C) +  '99' + String.fromCharCode(0x000C)",    -99,     -( String.fromCharCode(0x0009) +  '99' + String.fromCharCode(0x000C)) );
new TestCase( "-( String.fromCharCode(0x000D) +  '99' + String.fromCharCode(0x000D)",    -99,     -( String.fromCharCode(0x0009) +  '99' + String.fromCharCode(0x000D)) );
new TestCase( "-( String.fromCharCode(0x000B) +  '99' + String.fromCharCode(0x000B)",    -99,     -( String.fromCharCode(0x0009) +  '99' + String.fromCharCode(0x000B)) );
new TestCase( "-( String.fromCharCode(0x000A) +  '99' + String.fromCharCode(0x000A)",    -99,     -( String.fromCharCode(0x0009) +  '99' + String.fromCharCode(0x000A)) );

new TestCase( "-( '99' + String.fromCharCode(0x0009)",    -99,     -( '99' + String.fromCharCode(0x0009)) );
new TestCase( "-( '99' + String.fromCharCode(0x0020)",    -99,     -( '99' + String.fromCharCode(0x0020)) );
new TestCase( "-( '99' + String.fromCharCode(0x000C)",    -99,     -( '99' + String.fromCharCode(0x000C)) );
new TestCase( "-( '99' + String.fromCharCode(0x000D)",    -99,     -( '99' + String.fromCharCode(0x000D)) );
new TestCase( "-( '99' + String.fromCharCode(0x000B)",    -99,     -( '99' + String.fromCharCode(0x000B)) );
new TestCase( "-( '99' + String.fromCharCode(0x000A)",    -99,     -( '99' + String.fromCharCode(0x000A)) );

new TestCase( "-( String.fromCharCode(0x0009) +  99 )",    -99,     -( String.fromCharCode(0x0009) +  99 ) );
new TestCase( "-( String.fromCharCode(0x0020) +  99 )",    -99,     -( String.fromCharCode(0x0020) +  99 ) );
new TestCase( "-( String.fromCharCode(0x000C) +  99 )",    -99,     -( String.fromCharCode(0x000C) +  99 ) );
new TestCase( "-( String.fromCharCode(0x000B) +  99 )",    -99,     -( String.fromCharCode(0x000B) +  99 ) );
new TestCase( "-( String.fromCharCode(0x000D) +  99 )",    -99,     -( String.fromCharCode(0x000D) +  99 ) );
new TestCase( "-( String.fromCharCode(0x000A) +  99 )",    -99,     -( String.fromCharCode(0x000A) +  99 ) );

new TestCase( "-( String.fromCharCode(0x0009) +  99 + String.fromCharCode(0x0009)",    -99,     -( String.fromCharCode(0x0009) +  99 + String.fromCharCode(0x0009)) );
new TestCase( "-( String.fromCharCode(0x0020) +  99 + String.fromCharCode(0x0020)",    -99,     -( String.fromCharCode(0x0009) +  99 + String.fromCharCode(0x0020)) );
new TestCase( "-( String.fromCharCode(0x000C) +  99 + String.fromCharCode(0x000C)",    -99,     -( String.fromCharCode(0x0009) +  99 + String.fromCharCode(0x000C)) );
new TestCase( "-( String.fromCharCode(0x000D) +  99 + String.fromCharCode(0x000D)",    -99,     -( String.fromCharCode(0x0009) +  99 + String.fromCharCode(0x000D)) );
new TestCase( "-( String.fromCharCode(0x000B) +  99 + String.fromCharCode(0x000B)",    -99,     -( String.fromCharCode(0x0009) +  99 + String.fromCharCode(0x000B)) );
new TestCase( "-( String.fromCharCode(0x000A) +  99 + String.fromCharCode(0x000A)",    -99,     -( String.fromCharCode(0x0009) +  99 + String.fromCharCode(0x000A)) );

new TestCase( "-( 99 + String.fromCharCode(0x0009)",    -99,     -( 99 + String.fromCharCode(0x0009)) );
new TestCase( "-( 99 + String.fromCharCode(0x0020)",    -99,     -( 99 + String.fromCharCode(0x0020)) );
new TestCase( "-( 99 + String.fromCharCode(0x000C)",    -99,     -( 99 + String.fromCharCode(0x000C)) );
new TestCase( "-( 99 + String.fromCharCode(0x000D)",    -99,     -( 99 + String.fromCharCode(0x000D)) );
new TestCase( "-( 99 + String.fromCharCode(0x000B)",    -99,     -( 99 + String.fromCharCode(0x000B)) );
new TestCase( "-( 99 + String.fromCharCode(0x000A)",    -99,     -( 99 + String.fromCharCode(0x000A)) );


// StrNumericLiteral:::StrDecimalLiteral:::Infinity

new TestCase( "-('Infinity')",   -Math.pow(10,10000),   -("Infinity") );
new TestCase( "-('-Infinity')", +Math.pow(10,10000),   -("-Infinity") );
new TestCase( "-('+Infinity')",  -Math.pow(10,10000),   -("+Infinity") );

// StrNumericLiteral:::   StrDecimalLiteral ::: DecimalDigits . DecimalDigits opt ExponentPart opt

new TestCase( "-('0')",          -0,          -("0") );
new TestCase( "-('-0')",         +0,         -("-0") );
new TestCase( "-('+0')",          -0,         -("+0") );

new TestCase( "-('1')",          -1,          -("1") );
new TestCase( "-('-1')",         +1,         -("-1") );
new TestCase( "-('+1')",          -1,         -("+1") );

new TestCase( "-('2')",          -2,          -("2") );
new TestCase( "-('-2')",         +2,         -("-2") );
new TestCase( "-('+2')",          -2,         -("+2") );

new TestCase( "-('3')",          -3,          -("3") );
new TestCase( "-('-3')",         +3,         -("-3") );
new TestCase( "-('+3')",          -3,         -("+3") );

new TestCase( "-('4')",          -4,          -("4") );
new TestCase( "-('-4')",         +4,         -("-4") );
new TestCase( "-('+4')",          -4,         -("+4") );

new TestCase( "-('5')",          -5,          -("5") );
new TestCase( "-('-5')",         +5,         -("-5") );
new TestCase( "-('+5')",          -5,         -("+5") );

new TestCase( "-('6')",          -6,          -("6") );
new TestCase( "-('-6')",         +6,         -("-6") );
new TestCase( "-('+6')",          -6,         -("+6") );

new TestCase( "-('7')",          -7,          -("7") );
new TestCase( "-('-7')",         +7,         -("-7") );
new TestCase( "-('+7')",          -7,         -("+7") );

new TestCase( "-('8')",          -8,          -("8") );
new TestCase( "-('-8')",         +8,         -("-8") );
new TestCase( "-('+8')",          -8,         -("+8") );

new TestCase( "-('9')",          -9,          -("9") );
new TestCase( "-('-9')",         +9,         -("-9") );
new TestCase( "-('+9')",          -9,         -("+9") );

new TestCase( "-('3.14159')",    -3.14159,    -("3.14159") );
new TestCase( "-('-3.14159')",   +3.14159,   -("-3.14159") );
new TestCase( "-('+3.14159')",   -3.14159,    -("+3.14159") );

new TestCase( "-('3.')",         -3,          -("3.") );
new TestCase( "-('-3.')",        +3,         -("-3.") );
new TestCase( "-('+3.')",        -3,          -("+3.") );

new TestCase( "-('3.e1')",       -30,         -("3.e1") );
new TestCase( "-('-3.e1')",      +30,        -("-3.e1") );
new TestCase( "-('+3.e1')",      -30,         -("+3.e1") );

new TestCase( "-('3.e+1')",       -30,         -("3.e+1") );
new TestCase( "-('-3.e+1')",      +30,        -("-3.e+1") );
new TestCase( "-('+3.e+1')",      -30,         -("+3.e+1") );

new TestCase( "-('3.e-1')",       -.30,         -("3.e-1") );
new TestCase( "-('-3.e-1')",      +.30,        -("-3.e-1") );
new TestCase( "-('+3.e-1')",      -.30,         -("+3.e-1") );

// StrDecimalLiteral:::  .DecimalDigits ExponentPart opt

new TestCase( "-('.00001')",     -0.00001,    -(".00001") );
new TestCase( "-('+.00001')",    -0.00001,    -("+.00001") );
new TestCase( "-('-0.0001')",    +0.00001,   -("-.00001") );

new TestCase( "-('.01e2')",      -1,          -(".01e2") );
new TestCase( "-('+.01e2')",     -1,          -("+.01e2") );
new TestCase( "-('-.01e2')",     +1,         -("-.01e2") );

new TestCase( "-('.01e+2')",      -1,         -(".01e+2") );
new TestCase( "-('+.01e+2')",     -1,         -("+.01e+2") );
new TestCase( "-('-.01e+2')",     +1,        -("-.01e+2") );

new TestCase( "-('.01e-2')",      -0.0001,    -(".01e-2") );
new TestCase( "-('+.01e-2')",     -0.0001,    -("+.01e-2") );
new TestCase( "-('-.01e-2')",     +0.0001,   -("-.01e-2") );

//  StrDecimalLiteral:::    DecimalDigits ExponentPart opt

new TestCase( "-('1234e5')",     -123400000,  -("1234e5") );
new TestCase( "-('+1234e5')",    -123400000,  -("+1234e5") );
new TestCase( "-('-1234e5')",    +123400000, -("-1234e5") );

new TestCase( "-('1234e+5')",    -123400000,  -("1234e+5") );
new TestCase( "-('+1234e+5')",   -123400000,  -("+1234e+5") );
new TestCase( "-('-1234e+5')",   +123400000, -("-1234e+5") );

new TestCase( "-('1234e-5')",     -0.01234,  -("1234e-5") );
new TestCase( "-('+1234e-5')",    -0.01234,  -("+1234e-5") );
new TestCase( "-('-1234e-5')",    +0.01234, -("-1234e-5") );

// StrNumericLiteral::: HexIntegerLiteral

new TestCase( "-('0x0')",        -0,          -("0x0"));
new TestCase( "-('0x1')",        -1,          -("0x1"));
new TestCase( "-('0x2')",        -2,          -("0x2"));
new TestCase( "-('0x3')",        -3,          -("0x3"));
new TestCase( "-('0x4')",        -4,          -("0x4"));
new TestCase( "-('0x5')",        -5,          -("0x5"));
new TestCase( "-('0x6')",        -6,          -("0x6"));
new TestCase( "-('0x7')",        -7,          -("0x7"));
new TestCase( "-('0x8')",        -8,          -("0x8"));
new TestCase( "-('0x9')",        -9,          -("0x9"));
new TestCase( "-('0xa')",        -10,         -("0xa"));
new TestCase( "-('0xb')",        -11,         -("0xb"));
new TestCase( "-('0xc')",        -12,         -("0xc"));
new TestCase( "-('0xd')",        -13,         -("0xd"));
new TestCase( "-('0xe')",        -14,         -("0xe"));
new TestCase( "-('0xf')",        -15,         -("0xf"));
new TestCase( "-('0xA')",        -10,         -("0xA"));
new TestCase( "-('0xB')",        -11,         -("0xB"));
new TestCase( "-('0xC')",        -12,         -("0xC"));
new TestCase( "-('0xD')",        -13,         -("0xD"));
new TestCase( "-('0xE')",        -14,         -("0xE"));
new TestCase( "-('0xF')",        -15,         -("0xF"));

new TestCase( "-('0X0')",        -0,          -("0X0"));
new TestCase( "-('0X1')",        -1,          -("0X1"));
new TestCase( "-('0X2')",        -2,          -("0X2"));
new TestCase( "-('0X3')",        -3,          -("0X3"));
new TestCase( "-('0X4')",        -4,          -("0X4"));
new TestCase( "-('0X5')",        -5,          -("0X5"));
new TestCase( "-('0X6')",        -6,          -("0X6"));
new TestCase( "-('0X7')",        -7,          -("0X7"));
new TestCase( "-('0X8')",        -8,          -("0X8"));
new TestCase( "-('0X9')",        -9,          -("0X9"));
new TestCase( "-('0Xa')",        -10,         -("0Xa"));
new TestCase( "-('0Xb')",        -11,         -("0Xb"));
new TestCase( "-('0Xc')",        -12,         -("0Xc"));
new TestCase( "-('0Xd')",        -13,         -("0Xd"));
new TestCase( "-('0Xe')",        -14,         -("0Xe"));
new TestCase( "-('0Xf')",        -15,         -("0Xf"));
new TestCase( "-('0XA')",        -10,         -("0XA"));
new TestCase( "-('0XB')",        -11,         -("0XB"));
new TestCase( "-('0XC')",        -12,         -("0XC"));
new TestCase( "-('0XD')",        -13,         -("0XD"));
new TestCase( "-('0XE')",        -14,         -("0XE"));
new TestCase( "-('0XF')",        -15,         -("0XF"));

test();
