/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          7.7.3.js
   ECMA Section:       7.7.3 Numeric Literals

   Description:        A numeric literal stands for a value of the Number type
   This value is determined in two steps:  first a
   mathematical value (MV) is derived from the literal;
   second, this mathematical value is rounded, ideally
   using IEEE 754 round-to-nearest mode, to a reprentable
   value of of the number type.

   Author:             christine@netscape.com
   Date:               16 september 1997
*/
var SECTION = "7.7.3";
var TITLE   = "Numeric Literals";

writeHeaderToLog( SECTION + " "+ TITLE);

new TestCase( "0",     0,      0 );
new TestCase( "1",     1,      1 );
new TestCase( "2",     2,      2 );
new TestCase( "3",     3,      3 );
new TestCase( "4",     4,      4 );
new TestCase( "5",     5,      5 );
new TestCase( "6",     6,      6 );
new TestCase( "7",     7,      7 );
new TestCase( "8",     8,      8 );
new TestCase( "9",     9,      9 );

new TestCase( "0.",     0,      0. );
new TestCase( "1.",     1,      1. );
new TestCase( "2.",     2,      2. );
new TestCase( "3.",     3,      3. );
new TestCase( "4.",     4,      4. );

new TestCase( "0.e0",  0,      0.e0 );
new TestCase( "1.e1",  10,     1.e1 );
new TestCase( "2.e2",  200,    2.e2 );
new TestCase( "3.e3",  3000,   3.e3 );
new TestCase( "4.e4",  40000,  4.e4 );

new TestCase( "0.1e0",  .1,    0.1e0 );
new TestCase( "1.1e1",  11,    1.1e1 );
new TestCase( "2.2e2",  220,   2.2e2 );
new TestCase( "3.3e3",  3300,  3.3e3 );
new TestCase( "4.4e4",  44000, 4.4e4 );

new TestCase( ".1e0",  .1,   .1e0 );
new TestCase( ".1e1",  1,    .1e1 );
new TestCase( ".2e2",  20,   .2e2 );
new TestCase( ".3e3",  300,  .3e3 );
new TestCase( ".4e4",  4000, .4e4 );

new TestCase( "0e0",  0,     0e0 );
new TestCase( "1e1",  10,    1e1 );
new TestCase( "2e2",  200,   2e2 );
new TestCase( "3e3",  3000,  3e3 );
new TestCase( "4e4",  40000, 4e4 );

new TestCase( "0e0",  0,     0e0 );
new TestCase( "1e1",  10,    1e1 );
new TestCase( "2e2",  200,   2e2 );
new TestCase( "3e3",  3000,  3e3 );
new TestCase( "4e4",  40000, 4e4 );

new TestCase( "0E0",  0,     0E0 );
new TestCase( "1E1",  10,    1E1 );
new TestCase( "2E2",  200,   2E2 );
new TestCase( "3E3",  3000,  3E3 );
new TestCase( "4E4",  40000, 4E4 );

new TestCase( "1.e-1",  0.1,     1.e-1 );
new TestCase( "2.e-2",  0.02,    2.e-2 );
new TestCase( "3.e-3",  0.003,   3.e-3 );
new TestCase( "4.e-4",  0.0004,  4.e-4 );

new TestCase( "0.1e-0",  .1,     0.1e-0 );
new TestCase( "1.1e-1",  0.11,   1.1e-1 );
new TestCase( "2.2e-2",  .022,   2.2e-2 );
new TestCase( "3.3e-3",  .0033,  3.3e-3 );
new TestCase( "4.4e-4",  .00044, 4.4e-4 );

new TestCase( ".1e-0",  .1,    .1e-0 );
new TestCase( ".1e-1",  .01,    .1e-1 );
new TestCase( ".2e-2",  .002,   .2e-2 );
new TestCase( ".3e-3",  .0003,  .3e-3 );
new TestCase( ".4e-4",  .00004, .4e-4 );

new TestCase( "1.e+1",  10,     1.e+1 );
new TestCase( "2.e+2",  200,    2.e+2 );
new TestCase( "3.e+3",  3000,   3.e+3 );
new TestCase( "4.e+4",  40000,  4.e+4 );

new TestCase( "0.1e+0",  .1,    0.1e+0 );
new TestCase( "1.1e+1",  11,    1.1e+1 );
new TestCase( "2.2e+2",  220,   2.2e+2 );
new TestCase( "3.3e+3",  3300,  3.3e+3 );
new TestCase( "4.4e+4",  44000, 4.4e+4 );

new TestCase( ".1e+0",  .1,   .1e+0 );
new TestCase( ".1e+1",  1,    .1e+1 );
new TestCase( ".2e+2",  20,   .2e+2 );
new TestCase( ".3e+3",  300,  .3e+3 );
new TestCase( ".4e+4",  4000, .4e+4 );

new TestCase( "0x0",  0,   0x0 );
new TestCase( "0x1",  1,   0x1 );
new TestCase( "0x2",  2,   0x2 );
new TestCase( "0x3",  3,   0x3 );
new TestCase( "0x4",  4,   0x4 );
new TestCase( "0x5",  5,   0x5 );
new TestCase( "0x6",  6,   0x6 );
new TestCase( "0x7",  7,   0x7 );
new TestCase( "0x8",  8,   0x8 );
new TestCase( "0x9",  9,   0x9 );
new TestCase( "0xa",  10,  0xa );
new TestCase( "0xb",  11,  0xb );
new TestCase( "0xc",  12,  0xc );
new TestCase( "0xd",  13,  0xd );
new TestCase( "0xe",  14,  0xe );
new TestCase( "0xf",  15,  0xf );

new TestCase( "0X0",  0,   0X0 );
new TestCase( "0X1",  1,   0X1 );
new TestCase( "0X2",  2,   0X2 );
new TestCase( "0X3",  3,   0X3 );
new TestCase( "0X4",  4,   0X4 );
new TestCase( "0X5",  5,   0X5 );
new TestCase( "0X6",  6,   0X6 );
new TestCase( "0X7",  7,   0X7 );
new TestCase( "0X8",  8,   0X8 );
new TestCase( "0X9",  9,   0X9 );
new TestCase( "0Xa",  10,  0Xa );
new TestCase( "0Xb",  11,  0Xb );
new TestCase( "0Xc",  12,  0Xc );
new TestCase( "0Xd",  13,  0Xd );
new TestCase( "0Xe",  14,  0Xe );
new TestCase( "0Xf",  15,  0Xf );

new TestCase( "0x0",  0,   0x0 );
new TestCase( "0x1",  1,   0x1 );
new TestCase( "0x2",  2,   0x2 );
new TestCase( "0x3",  3,   0x3 );
new TestCase( "0x4",  4,   0x4 );
new TestCase( "0x5",  5,   0x5 );
new TestCase( "0x6",  6,   0x6 );
new TestCase( "0x7",  7,   0x7 );
new TestCase( "0x8",  8,   0x8 );
new TestCase( "0x9",  9,   0x9 );
new TestCase( "0xA",  10,  0xA );
new TestCase( "0xB",  11,  0xB );
new TestCase( "0xC",  12,  0xC );
new TestCase( "0xD",  13,  0xD );
new TestCase( "0xE",  14,  0xE );
new TestCase( "0xF",  15,  0xF );

new TestCase( "0X0",  0,   0X0 );
new TestCase( "0X1",  1,   0X1 );
new TestCase( "0X2",  2,   0X2 );
new TestCase( "0X3",  3,   0X3 );
new TestCase( "0X4",  4,   0X4 );
new TestCase( "0X5",  5,   0X5 );
new TestCase( "0X6",  6,   0X6 );
new TestCase( "0X7",  7,   0X7 );
new TestCase( "0X8",  8,   0X8 );
new TestCase( "0X9",  9,   0X9 );
new TestCase( "0XA",  10,  0XA );
new TestCase( "0XB",  11,  0XB );
new TestCase( "0XC",  12,  0XC );
new TestCase( "0XD",  13,  0XD );
new TestCase( "0XE",  14,  0XE );
new TestCase( "0XF",  15,  0XF );


new TestCase( "00",  0,   00 );
new TestCase( "01",  1,   01 );
new TestCase( "02",  2,   02 );
new TestCase( "03",  3,   03 );
new TestCase( "04",  4,   04 );
new TestCase( "05",  5,   05 );
new TestCase( "06",  6,   06 );
new TestCase( "07",  7,   07 );

new TestCase( "000",  0,   000 );
new TestCase( "011",  9,   011 );
new TestCase( "022",  18,  022 );
new TestCase( "033",  27,  033 );
new TestCase( "044",  36,  044 );
new TestCase( "055",  45,  055 );
new TestCase( "066",  54,  066 );
new TestCase( "077",  63,   077 );

new TestCase( "0.00000000001",  0.00000000001,  0.00000000001 );
new TestCase( "0.00000000001e-2",  0.0000000000001,  0.00000000001e-2 );


new TestCase( "123456789012345671.9999",
	      "123456789012345660",
	      123456789012345671.9999 +"");
new TestCase( "123456789012345672",
	      "123456789012345660",
	      123456789012345672 +"");

new TestCase(   "123456789012345672.000000000000000000000000000",
		"123456789012345660",
		123456789012345672.000000000000000000000000000 +"");

new TestCase( "123456789012345672.01",
	      "123456789012345680",
	      123456789012345672.01 +"");

new TestCase( "123456789012345672.000000000000000000000000001+'' == 123456789012345680 || 123456789012345660",
	      true,
	      ( 123456789012345672.00000000000000000000000000 +""  == 1234567890 * 100000000 + 12345680 )
	      ||
	      ( 123456789012345672.00000000000000000000000000 +""  == 1234567890 * 100000000 + 12345660) );

new TestCase( "123456789012345673",
	      "123456789012345680",
	      123456789012345673 +"" );

new TestCase( "-123456789012345671.9999",
	      "-123456789012345660",
	      -123456789012345671.9999 +"" );

new TestCase( "-123456789012345672",
	      "-123456789012345660",
	      -123456789012345672+"");

new TestCase( "-123456789012345672.000000000000000000000000000",
	      "-123456789012345660",
	      -123456789012345672.000000000000000000000000000 +"");

new TestCase( "-123456789012345672.01",
	      "-123456789012345680",
	      -123456789012345672.01 +"" );

new TestCase( "-123456789012345672.000000000000000000000000001 == -123456789012345680 or -123456789012345660",
	      true,
	      (-123456789012345672.000000000000000000000000001 +"" == -1234567890 * 100000000 -12345680)
	      ||
	      (-123456789012345672.000000000000000000000000001 +"" == -1234567890 * 100000000 -12345660));

new TestCase( -123456789012345673,
	      "-123456789012345680",
	      -123456789012345673 +"");

new TestCase( "12345678901234567890",
	      "12345678901234567000",
	      12345678901234567890 +"" );


/*
  new TestCase( "12345678901234567",         "12345678901234567",        12345678901234567+"" );
  new TestCase( "123456789012345678",        "123456789012345678",       123456789012345678+"" );
  new TestCase( "1234567890123456789",       "1234567890123456789",      1234567890123456789+"" );
  new TestCase( "12345678901234567890",      "12345678901234567890",     12345678901234567890+"" );
  new TestCase( "123456789012345678900",     "123456789012345678900",    123456789012345678900+"" );
  new TestCase( "1234567890123456789000",    "1234567890123456789000",   1234567890123456789000+"" );
*/
new TestCase( "0x1",          1,          0x1 );
new TestCase( "0x10",         16,         0x10 );
new TestCase( "0x100",        256,        0x100 );
new TestCase( "0x1000",       4096,       0x1000 );
new TestCase( "0x10000",      65536,      0x10000 );
new TestCase( "0x100000",     1048576,    0x100000 );
new TestCase( "0x1000000",    16777216,   0x1000000 );
new TestCase( "0x10000000",   268435456,  0x10000000 );
/*
  new TestCase( "0x100000000",          4294967296,      0x100000000 );
  new TestCase( "0x1000000000",         68719476736,     0x1000000000 );
  new TestCase( "0x10000000000",        1099511627776,     0x10000000000 );
*/

test();
