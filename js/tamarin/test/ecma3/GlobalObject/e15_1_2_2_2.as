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
var SECTION = "15.1.2.2-2";
var VERSION = "ECMA_1";
    startTest();
var TITLE   = "parseInt(string, radix)";
var BUGNUMBER="123874";

writeHeaderToLog( SECTION + " "+ TITLE);

var testcases = new Array();

testcases[tc++] = new TestCase( SECTION,
    'parseInt("000000100000000100100011010001010110011110001001101010111100",2)',
    9027215253084860,
    parseInt("000000100000000100100011010001010110011110001001101010111100",2) );

testcases[tc++] = new TestCase( SECTION,
    'parseInt("000000100000000100100011010001010110011110001001101010111101",2)',
    9027215253084860,
    parseInt("000000100000000100100011010001010110011110001001101010111101",2));

testcases[tc++] = new TestCase( SECTION,
    'parseInt("000000100000000100100011010001010110011110001001101010111111",2)',
    9027215253084864,
    parseInt("000000100000000100100011010001010110011110001001101010111111",2) );

testcases[tc++] = new TestCase( SECTION,
    'parseInt("0000001000000001001000110100010101100111100010011010101111010",2)',
    18054430506169720,
    parseInt("0000001000000001001000110100010101100111100010011010101111010",2) );

testcases[tc++] = new TestCase( SECTION,
    'parseInt("0000001000000001001000110100010101100111100010011010101111011",2)',
    18054430506169724,
    parseInt("0000001000000001001000110100010101100111100010011010101111011",2));

testcases[tc++] = new TestCase( SECTION,
    'parseInt("0000001000000001001000110100010101100111100010011010101111100",2)',
    18054430506169724,
    parseInt("0000001000000001001000110100010101100111100010011010101111100",2) );

testcases[tc++] = new TestCase( SECTION,
    'parseInt("0000001000000001001000110100010101100111100010011010101111110",2)',
    18054430506169728,
    parseInt("0000001000000001001000110100010101100111100010011010101111110",2) );

testcases[tc++] = new TestCase( SECTION,
    'parseInt("yz",35)',
    34,
    parseInt("yz",35) );

testcases[tc++] = new TestCase( SECTION,
    'parseInt("yz",36)',
    1259,
    parseInt("yz",36) );

testcases[tc++] = new TestCase( SECTION,
    'parseInt("yz",37)',
    NaN,
    parseInt("yz",37) );

testcases[tc++] = new TestCase( SECTION,
    'parseInt("+77")',
    77,
    parseInt("+77") );

testcases[tc++] = new TestCase( SECTION,
    'parseInt("-77",9)',
    -70,
    parseInt("-77",9) );

testcases[tc++] = new TestCase( SECTION,
    'parseInt("\u20001234\u2000")',
    1234,
    parseInt("\u20001234\u2000") );

testcases[tc++] = new TestCase( SECTION,
    'parseInt("123456789012345678")',
    123456789012345700,
    parseInt("123456789012345678") );

testcases[tc++] = new TestCase( SECTION,
    'parseInt("9",8)',
    NaN,
    parseInt("9",8) );

testcases[tc++] = new TestCase( SECTION,
    'parseInt("1e2")',
    1,
    parseInt("1e2") );

testcases[tc++] = new TestCase( SECTION,
    'parseInt("1.9999999999999999999")',
    1,
    parseInt("1.9999999999999999999") );

testcases[tc++] = new TestCase( SECTION,
    'parseInt("0x10")',
    16,
    parseInt("0x10") );

testcases[tc++] = new TestCase( SECTION,
    'parseInt("0x10",10)',
    0,
    parseInt("0x10",10));

testcases[tc++] = new TestCase( SECTION,
    'parseInt("0022")',
    22,
    parseInt("0022"));

testcases[tc++] = new TestCase( SECTION,
    'parseInt("0022",10)',
    22,
    parseInt("0022",10) );

testcases[tc++] = new TestCase( SECTION,
    'parseInt("0x1000000000000080")',
    1152921504606847000,
    parseInt("0x1000000000000080") );

testcases[tc++] = new TestCase( SECTION,
    'parseInt("0x1000000000000081")',
    1152921504606847200,
    parseInt("0x1000000000000081") );

s =
"0xFFFFFFFFFFFFF80000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"

s += "0000000000000000000000000000000000000";

testcases[tc++] = new TestCase( SECTION,
    "s = " + s +"; -s",
    -1.7976931348623157e+308,
    -s );

s =
"0xFFFFFFFFFFFFF80000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
s += "0000000000000000000000000000000000001";

testcases[tc++] = new TestCase( SECTION,
    "s = " + s +"; -s",
    -1.7976931348623157e+308,
    -s );


s = "0xFFFFFFFFFFFFFC0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";

s += "0000000000000000000000000000000000000"


testcases[tc++] = new TestCase( SECTION,
    "s = " + s + "; -s",
    -Infinity,
    -s );

s = "0xFFFFFFFFFFFFFB0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000";
s += "0000000000000000000000000000000000001";

testcases[tc++] = new TestCase( SECTION,
    "s = " + s + "; -s",
    -1.7976931348623157e+308,
    -s );

s += "0"

testcases[tc++] = new TestCase( SECTION,
    "s = " + s + "; -s",
    -Infinity,
    -s );

testcases[tc++] = new TestCase( SECTION,
    'parseInt(s)',
    Infinity,
    parseInt(s) );

testcases[tc++] = new TestCase( SECTION,
    'parseInt(s,32)',
    0,
    parseInt(s,32) );

testcases[tc++] = new TestCase( SECTION,
    'parseInt(s,36)',
    Infinity,
    parseInt(s,36));

test();
