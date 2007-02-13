/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
    var SECTION = "7.7.3";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Numeric Literals";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();


function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION, "0",     0,      0 );
    array[item++] = new TestCase( SECTION, "1",     1,      1 );
    array[item++] = new TestCase( SECTION, "2",     2,      2 );
    array[item++] = new TestCase( SECTION, "3",     3,      3 );
    array[item++] = new TestCase( SECTION, "4",     4,      4 );
    array[item++] = new TestCase( SECTION, "5",     5,      5 );
    array[item++] = new TestCase( SECTION, "6",     6,      6 );
    array[item++] = new TestCase( SECTION, "7",     7,      7 );
    array[item++] = new TestCase( SECTION, "8",     8,      8 );
    array[item++] = new TestCase( SECTION, "9",     9,      9 );

    array[item++] = new TestCase( SECTION, "0.",     0,      0. );
    array[item++] = new TestCase( SECTION, "1.",     1,      1. );
    array[item++] = new TestCase( SECTION, "2.",     2,      2. );
    array[item++] = new TestCase( SECTION, "3.",     3,      3. );
    array[item++] = new TestCase( SECTION, "4.",     4,      4. );

    array[item++] = new TestCase( SECTION, "0.e0",  0,      0.e0 );
    array[item++] = new TestCase( SECTION, "1.e1",  10,     1.e1 );
    array[item++] = new TestCase( SECTION, "2.e2",  200,    2.e2 );
    array[item++] = new TestCase( SECTION, "3.e3",  3000,   3.e3 );
    array[item++] = new TestCase( SECTION, "4.e4",  40000,  4.e4 );

    array[item++] = new TestCase( SECTION, "0.1e0",  .1,    0.1e0 );
    array[item++] = new TestCase( SECTION, "1.1e1",  11,    1.1e1 );
    array[item++] = new TestCase( SECTION, "2.2e2",  220,   2.2e2 );
    array[item++] = new TestCase( SECTION, "3.3e3",  3300,  3.3e3 );
    array[item++] = new TestCase( SECTION, "4.4e4",  44000, 4.4e4 );

    array[item++] = new TestCase( SECTION, ".1e0",  .1,   .1e0 );
    array[item++] = new TestCase( SECTION, ".1e1",  1,    .1e1 );
    array[item++] = new TestCase( SECTION, ".2e2",  20,   .2e2 );
    array[item++] = new TestCase( SECTION, ".3e3",  300,  .3e3 );
    array[item++] = new TestCase( SECTION, ".4e4",  4000, .4e4 );

    array[item++] = new TestCase( SECTION, "0e0",  0,     0e0 );
    array[item++] = new TestCase( SECTION, "1e1",  10,    1e1 );
    array[item++] = new TestCase( SECTION, "2e2",  200,   2e2 );
    array[item++] = new TestCase( SECTION, "3e3",  3000,  3e3 );
    array[item++] = new TestCase( SECTION, "4e4",  40000, 4e4 );

    array[item++] = new TestCase( SECTION, "0e0",  0,     0e0 );
    array[item++] = new TestCase( SECTION, "1e1",  10,    1e1 );
    array[item++] = new TestCase( SECTION, "2e2",  200,   2e2 );
    array[item++] = new TestCase( SECTION, "3e3",  3000,  3e3 );
    array[item++] = new TestCase( SECTION, "4e4",  40000, 4e4 );

    array[item++] = new TestCase( SECTION, "0E0",  0,     0E0 );
    array[item++] = new TestCase( SECTION, "1E1",  10,    1E1 );
    array[item++] = new TestCase( SECTION, "2E2",  200,   2E2 );
    array[item++] = new TestCase( SECTION, "3E3",  3000,  3E3 );
    array[item++] = new TestCase( SECTION, "4E4",  40000, 4E4 );

    array[item++] = new TestCase( SECTION, "1.e-1",  0.1,     1.e-1 );
    array[item++] = new TestCase( SECTION, "2.e-2",  0.02,    2.e-2 );
    array[item++] = new TestCase( SECTION, "3.e-3",  0.003,   3.e-3 );
    array[item++] = new TestCase( SECTION, "4.e-4",  0.0004,  4.e-4 );

    array[item++] = new TestCase( SECTION, "0.1e-0",  .1,     0.1e-0 );
    array[item++] = new TestCase( SECTION, "1.1e-1",  0.11,   1.1e-1 );
    array[item++] = new TestCase( SECTION, "2.2e-2",  .022,   2.2e-2 );
    array[item++] = new TestCase( SECTION, "3.3e-3",  .0033,  3.3e-3 );
    array[item++] = new TestCase( SECTION, "4.4e-4",  .00044, 4.4e-4 );

    array[item++] = new TestCase( SECTION, ".1e-0",  .1,    .1e-0 );
    array[item++] = new TestCase( SECTION, ".1e-1",  .01,    .1e-1 );
    array[item++] = new TestCase( SECTION, ".2e-2",  .002,   .2e-2 );
    array[item++] = new TestCase( SECTION, ".3e-3",  .0003,  .3e-3 );
    array[item++] = new TestCase( SECTION, ".4e-4",  .00004, .4e-4 );

    array[item++] = new TestCase( SECTION, "1.e+1",  10,     1.e+1 );
    array[item++] = new TestCase( SECTION, "2.e+2",  200,    2.e+2 );
    array[item++] = new TestCase( SECTION, "3.e+3",  3000,   3.e+3 );
    array[item++] = new TestCase( SECTION, "4.e+4",  40000,  4.e+4 );

    array[item++] = new TestCase( SECTION, "0.1e+0",  .1,    0.1e+0 );
    array[item++] = new TestCase( SECTION, "1.1e+1",  11,    1.1e+1 );
    array[item++] = new TestCase( SECTION, "2.2e+2",  220,   2.2e+2 );
    array[item++] = new TestCase( SECTION, "3.3e+3",  3300,  3.3e+3 );
    array[item++] = new TestCase( SECTION, "4.4e+4",  44000, 4.4e+4 );

    array[item++] = new TestCase( SECTION, ".1e+0",  .1,   .1e+0 );
    array[item++] = new TestCase( SECTION, ".1e+1",  1,    .1e+1 );
    array[item++] = new TestCase( SECTION, ".2e+2",  20,   .2e+2 );
    array[item++] = new TestCase( SECTION, ".3e+3",  300,  .3e+3 );
    array[item++] = new TestCase( SECTION, ".4e+4",  4000, .4e+4 );

    array[item++] = new TestCase( SECTION, "0x0",  0,   0x0 );
    array[item++] = new TestCase( SECTION, "0x1",  1,   0x1 );
    array[item++] = new TestCase( SECTION, "0x2",  2,   0x2 );
    array[item++] = new TestCase( SECTION, "0x3",  3,   0x3 );
    array[item++] = new TestCase( SECTION, "0x4",  4,   0x4 );
    array[item++] = new TestCase( SECTION, "0x5",  5,   0x5 );
    array[item++] = new TestCase( SECTION, "0x6",  6,   0x6 );
    array[item++] = new TestCase( SECTION, "0x7",  7,   0x7 );
    array[item++] = new TestCase( SECTION, "0x8",  8,   0x8 );
    array[item++] = new TestCase( SECTION, "0x9",  9,   0x9 );
    array[item++] = new TestCase( SECTION, "0xa",  10,  0xa );
    array[item++] = new TestCase( SECTION, "0xb",  11,  0xb );
    array[item++] = new TestCase( SECTION, "0xc",  12,  0xc );
    array[item++] = new TestCase( SECTION, "0xd",  13,  0xd );
    array[item++] = new TestCase( SECTION, "0xe",  14,  0xe );
    array[item++] = new TestCase( SECTION, "0xf",  15,  0xf );

    array[item++] = new TestCase( SECTION, "0X0",  0,   0X0 );
    array[item++] = new TestCase( SECTION, "0X1",  1,   0X1 );
    array[item++] = new TestCase( SECTION, "0X2",  2,   0X2 );
    array[item++] = new TestCase( SECTION, "0X3",  3,   0X3 );
    array[item++] = new TestCase( SECTION, "0X4",  4,   0X4 );
    array[item++] = new TestCase( SECTION, "0X5",  5,   0X5 );
    array[item++] = new TestCase( SECTION, "0X6",  6,   0X6 );
    array[item++] = new TestCase( SECTION, "0X7",  7,   0X7 );
    array[item++] = new TestCase( SECTION, "0X8",  8,   0X8 );
    array[item++] = new TestCase( SECTION, "0X9",  9,   0X9 );
    array[item++] = new TestCase( SECTION, "0Xa",  10,  0Xa );
    array[item++] = new TestCase( SECTION, "0Xb",  11,  0Xb );
    array[item++] = new TestCase( SECTION, "0Xc",  12,  0Xc );
    array[item++] = new TestCase( SECTION, "0Xd",  13,  0Xd );
    array[item++] = new TestCase( SECTION, "0Xe",  14,  0Xe );
    array[item++] = new TestCase( SECTION, "0Xf",  15,  0Xf );

    array[item++] = new TestCase( SECTION, "0x0",  0,   0x0 );
    array[item++] = new TestCase( SECTION, "0x1",  1,   0x1 );
    array[item++] = new TestCase( SECTION, "0x2",  2,   0x2 );
    array[item++] = new TestCase( SECTION, "0x3",  3,   0x3 );
    array[item++] = new TestCase( SECTION, "0x4",  4,   0x4 );
    array[item++] = new TestCase( SECTION, "0x5",  5,   0x5 );
    array[item++] = new TestCase( SECTION, "0x6",  6,   0x6 );
    array[item++] = new TestCase( SECTION, "0x7",  7,   0x7 );
    array[item++] = new TestCase( SECTION, "0x8",  8,   0x8 );
    array[item++] = new TestCase( SECTION, "0x9",  9,   0x9 );
    array[item++] = new TestCase( SECTION, "0xA",  10,  0xA );
    array[item++] = new TestCase( SECTION, "0xB",  11,  0xB );
    array[item++] = new TestCase( SECTION, "0xC",  12,  0xC );
    array[item++] = new TestCase( SECTION, "0xD",  13,  0xD );
    array[item++] = new TestCase( SECTION, "0xE",  14,  0xE );
    array[item++] = new TestCase( SECTION, "0xF",  15,  0xF );

    array[item++] = new TestCase( SECTION, "0X0",  0,   0X0 );
    array[item++] = new TestCase( SECTION, "0X1",  1,   0X1 );
    array[item++] = new TestCase( SECTION, "0X2",  2,   0X2 );
    array[item++] = new TestCase( SECTION, "0X3",  3,   0X3 );
    array[item++] = new TestCase( SECTION, "0X4",  4,   0X4 );
    array[item++] = new TestCase( SECTION, "0X5",  5,   0X5 );
    array[item++] = new TestCase( SECTION, "0X6",  6,   0X6 );
    array[item++] = new TestCase( SECTION, "0X7",  7,   0X7 );
    array[item++] = new TestCase( SECTION, "0X8",  8,   0X8 );
    array[item++] = new TestCase( SECTION, "0X9",  9,   0X9 );
    array[item++] = new TestCase( SECTION, "0XA",  10,  0XA );
    array[item++] = new TestCase( SECTION, "0XB",  11,  0XB );
    array[item++] = new TestCase( SECTION, "0XC",  12,  0XC );
    array[item++] = new TestCase( SECTION, "0XD",  13,  0XD );
    array[item++] = new TestCase( SECTION, "0XE",  14,  0XE );
    array[item++] = new TestCase( SECTION, "0XF",  15,  0XF );


	// we will not support octal for flash 8
    array[item++] = new TestCase( SECTION, "00",  0,   00 );
    array[item++] = new TestCase( SECTION, "01",  1,   01 );
    array[item++] = new TestCase( SECTION, "02",  2,   02 );
    array[item++] = new TestCase( SECTION, "03",  3,   03 );
    array[item++] = new TestCase( SECTION, "04",  4,   04 );
    array[item++] = new TestCase( SECTION, "05",  5,   05 );
    array[item++] = new TestCase( SECTION, "06",  6,   06 );
    array[item++] = new TestCase( SECTION, "07",  7,   07 );

    array[item++] = new TestCase( SECTION, "000",  0,   000 );
    array[item++] = new TestCase( SECTION, "011",  11,   011 );
    array[item++] = new TestCase( SECTION, "022",  22,  022 );
    array[item++] = new TestCase( SECTION, "033",  33,  033 );
    array[item++] = new TestCase( SECTION, "044",  44,  044 );
    array[item++] = new TestCase( SECTION, "055",  55,  055 );
    array[item++] = new TestCase( SECTION, "066",  66,  066 );
    array[item++] = new TestCase( SECTION, "077",  77,   077 );

    array[item++] = new TestCase( SECTION, "0.00000000001",  0.00000000001,  0.00000000001 );
    array[item++] = new TestCase( SECTION, "0.00000000001e-2",  0.0000000000001,  0.00000000001e-2 );


    array[item++] = new TestCase( SECTION,
                                  "123456789012345671.9999",
                                  "123456789012345660",
                                  123456789012345671.9999 +"");
    array[item++] = new TestCase( SECTION,
                               "123456789012345672",
                               "123456789012345660",
                               123456789012345672 +"");

    array[item++] = new TestCase(   SECTION,
                                    "123456789012345672.000000000000000000000000000",
                                   "123456789012345660",
                                    123456789012345672.000000000000000000000000000 +"");

    array[item++] = new TestCase( SECTION,
           "123456789012345672.01",
           "123456789012345680",
           123456789012345672.01 +"");

    array[item++] = new TestCase( SECTION,
           "123456789012345672.000000000000000000000000001+'' == 123456789012345680 || 123456789012345660",
           true,
           ( 123456789012345672.00000000000000000000000000 +""  == 1234567890 * 100000000 + 12345680 )
           ||
           ( 123456789012345672.00000000000000000000000000 +""  == 1234567890 * 100000000 + 12345660) );

    array[item++] = new TestCase( SECTION,
           "123456789012345673",
           "123456789012345680",
           123456789012345673 +"" );

    array[item++] = new TestCase( SECTION,
           "-123456789012345671.9999",
           "-123456789012345660",
           -123456789012345671.9999 +"" );

    array[item++] = new TestCase( SECTION,
           "-123456789012345672",
           "-123456789012345660",
           -123456789012345672+"");

    array[item++] = new TestCase( SECTION,
           "-123456789012345672.000000000000000000000000000",
           "-123456789012345660",
           -123456789012345672.000000000000000000000000000 +"");

    array[item++] = new TestCase( SECTION,
           "-123456789012345672.01",
           "-123456789012345680",
           -123456789012345672.01 +"" );

    array[item++] = new TestCase( SECTION,
           "-123456789012345672.000000000000000000000000001 == -123456789012345680 or -123456789012345660",
           true,
           (-123456789012345672.000000000000000000000000001 == -1234567890 * 100000000 -12345680)
           ||
           (-123456789012345672.000000000000000000000000001 == -1234567890 * 100000000 -12345660));

    array[item++] = new TestCase( SECTION,
           -123456789012345673,
           "-123456789012345680",
           -123456789012345673 +"");

    array[item++] = new TestCase( SECTION,
           "12345678901234567890",
           "12345678901234567000",
           12345678901234567890 +"" );



  /*array[item++] = new TestCase( SECTION, "12345678901234567",         "12345678901234567",        12345678901234567+"" );
    array[item++] = new TestCase( SECTION, "123456789012345678",        "123456789012345678",       123456789012345678+"" );
    array[item++] = new TestCase( SECTION, "1234567890123456789",       "1234567890123456789",      1234567890123456789+"" );
    array[item++] = new TestCase( SECTION, "12345678901234567890",      "12345678901234567890",     12345678901234567890+"" );
    array[item++] = new TestCase( SECTION, "123456789012345678900",     "123456789012345678900",    123456789012345678900+"" );
    array[item++] = new TestCase( SECTION, "1234567890123456789000",    "1234567890123456789000",   1234567890123456789000+"" );*/

    array[item++] =  new TestCase( SECTION, "0x1",          1,          0x1 );
    array[item++] =  new TestCase( SECTION, "0x10",         16,         0x10 );
    array[item++] =  new TestCase( SECTION, "0x100",        256,        0x100 );
    array[item++] =  new TestCase( SECTION, "0x1000",       4096,       0x1000 );
    array[item++] =  new TestCase( SECTION, "0x10000",      65536,      0x10000 );
    array[item++] =  new TestCase( SECTION, "0x100000",     1048576,    0x100000 );
    array[item++] =  new TestCase( SECTION, "0x1000000",    16777216,   0x1000000 );
    array[item++] =  new TestCase( SECTION, "0x10000000",   268435456,  0x10000000 );

    array[item++] =  new TestCase( SECTION, "0x100000000",          4294967296,      0x100000000 );
    array[item++] =  new TestCase( SECTION, "0x1000000000",         68719476736,     0x1000000000 );
    array[item++] =  new TestCase( SECTION, "0x10000000000",        1099511627776,     0x10000000000 );

    return ( array );
}
