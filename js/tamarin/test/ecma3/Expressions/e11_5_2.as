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
    var SECTION = "e11_5_2";
    var VERSION = "ECMA_1";
    startTest();
    var testcases = getTestCases();
    var BUGNUMBER="111202";

    writeHeaderToLog( SECTION + " Applying the / operator");
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

   // if either operand is NaN, the result is NaN.

    array[item++] = new TestCase( SECTION,    "Number.NaN / Number.NaN",    Number.NaN,     Number.NaN / Number.NaN );
    array[item++] = new TestCase( SECTION,    "Number.NaN / 1",             Number.NaN,     Number.NaN / 1 );
    array[item++] = new TestCase( SECTION,    "1 / Number.NaN",             Number.NaN,     1 / Number.NaN );

    array[item++] = new TestCase( SECTION,    "Number.POSITIVE_INFINITY / Number.NaN",    Number.NaN,     Number.POSITIVE_INFINITY / Number.NaN );
    array[item++] = new TestCase( SECTION,    "Number.NEGATIVE_INFINITY / Number.NaN",    Number.NaN,     Number.NEGATIVE_INFINITY / Number.NaN );

    // Division of an infinity by an infinity results in NaN.

    array[item++] = new TestCase( SECTION,    "Number.NEGATIVE_INFINITY / Number.NEGATIVE_INFINITY",    Number.NaN,   Number.NEGATIVE_INFINITY / Number.NEGATIVE_INFINITY );
    array[item++] = new TestCase( SECTION,    "Number.POSITIVE_INFINITY / Number.NEGATIVE_INFINITY",    Number.NaN,   Number.POSITIVE_INFINITY / Number.NEGATIVE_INFINITY );
    array[item++] = new TestCase( SECTION,    "Number.NEGATIVE_INFINITY / Number.POSITIVE_INFINITY",    Number.NaN,   Number.NEGATIVE_INFINITY / Number.POSITIVE_INFINITY );
    array[item++] = new TestCase( SECTION,    "Number.POSITIVE_INFINITY / Number.POSITIVE_INFINITY",    Number.NaN,   Number.POSITIVE_INFINITY / Number.POSITIVE_INFINITY );

    // Division of an infinity by a zero results in an infinity.

    array[item++] = new TestCase( SECTION,    "Number.POSITIVE_INFINITY / 0",   Number.POSITIVE_INFINITY, Number.POSITIVE_INFINITY / 0 );
    array[item++] = new TestCase( SECTION,    "Number.NEGATIVE_INFINITY / 0",   Number.NEGATIVE_INFINITY, Number.NEGATIVE_INFINITY / 0 );
    array[item++] = new TestCase( SECTION,    "Number.POSITIVE_INFINITY / -0",  Number.NEGATIVE_INFINITY,   Number.POSITIVE_INFINITY / -0 );
    array[item++] = new TestCase( SECTION,    "Number.NEGATIVE_INFINITY / -0",  Number.POSITIVE_INFINITY,   Number.NEGATIVE_INFINITY / -0 );

    // Division of an infinity by a non-zero finite value results in a signed infinity.

    array[item++] = new TestCase( SECTION,    "Number.NEGATIVE_INFINITY / 1 ",          Number.NEGATIVE_INFINITY,   Number.NEGATIVE_INFINITY / 1 );
    array[item++] = new TestCase( SECTION,    "Number.NEGATIVE_INFINITY / -1 ",         Number.POSITIVE_INFINITY,   Number.NEGATIVE_INFINITY / -1 );
    array[item++] = new TestCase( SECTION,    "Number.POSITIVE_INFINITY / 1 ",          Number.POSITIVE_INFINITY,   Number.POSITIVE_INFINITY / 1 );
    array[item++] = new TestCase( SECTION,    "Number.POSITIVE_INFINITY / -1 ",         Number.NEGATIVE_INFINITY,   Number.POSITIVE_INFINITY / -1 );

    array[item++] = new TestCase( SECTION,    "Number.NEGATIVE_INFINITY / Number.MAX_VALUE ",          Number.NEGATIVE_INFINITY,   Number.NEGATIVE_INFINITY / Number.MAX_VALUE );
    array[item++] = new TestCase( SECTION,    "Number.NEGATIVE_INFINITY / -Number.MAX_VALUE ",         Number.POSITIVE_INFINITY,   Number.NEGATIVE_INFINITY / -Number.MAX_VALUE );
    array[item++] = new TestCase( SECTION,    "Number.POSITIVE_INFINITY / Number.MAX_VALUE ",          Number.POSITIVE_INFINITY,   Number.POSITIVE_INFINITY / Number.MAX_VALUE );
    array[item++] = new TestCase( SECTION,    "Number.POSITIVE_INFINITY / -Number.MAX_VALUE ",         Number.NEGATIVE_INFINITY,   Number.POSITIVE_INFINITY / -Number.MAX_VALUE );

    // Division of a finite value by an infinity results in zero.

    array[item++] = new TestCase( SECTION,    "1 / Number.NEGATIVE_INFINITY",   -0,             1 / Number.NEGATIVE_INFINITY );
    array[item++] = new TestCase( SECTION,    "1 / Number.POSITIVE_INFINITY",   0,              1 / Number.POSITIVE_INFINITY );
    array[item++] = new TestCase( SECTION,    "-1 / Number.POSITIVE_INFINITY",  -0,             -1 / Number.POSITIVE_INFINITY );
    array[item++] = new TestCase( SECTION,    "-1 / Number.NEGATIVE_INFINITY",  0,              -1 / Number.NEGATIVE_INFINITY );

    array[item++] = new TestCase( SECTION,    "Number.MAX_VALUE / Number.NEGATIVE_INFINITY",   -0,             Number.MAX_VALUE / Number.NEGATIVE_INFINITY );
    array[item++] = new TestCase( SECTION,    "Number.MAX_VALUE / Number.POSITIVE_INFINITY",   0,              Number.MAX_VALUE / Number.POSITIVE_INFINITY );
    array[item++] = new TestCase( SECTION,    "-Number.MAX_VALUE / Number.POSITIVE_INFINITY",  -0,             -Number.MAX_VALUE / Number.POSITIVE_INFINITY );
    array[item++] = new TestCase( SECTION,    "-Number.MAX_VALUE / Number.NEGATIVE_INFINITY",  0,              -Number.MAX_VALUE / Number.NEGATIVE_INFINITY );

    // Division of a zero by a zero results in NaN

    array[item++] = new TestCase( SECTION,    "0 / -0",                         Number.NaN,     0 / -0 );
    array[item++] = new TestCase( SECTION,    "-0 / 0",                         Number.NaN,     -0 / 0 );
    array[item++] = new TestCase( SECTION,    "-0 / -0",                        Number.NaN,     -0 / -0 );
    array[item++] = new TestCase( SECTION,    "0 / 0",                          Number.NaN,     0 / 0 );

    // division of zero by any other finite value results in zero

    array[item++] = new TestCase( SECTION,    "0 / 1",                          0,              0 / 1 );
    array[item++] = new TestCase( SECTION,    "0 / -1",                        -0,              0 / -1 );
    array[item++] = new TestCase( SECTION,    "-0 / 1",                        -0,              -0 / 1 );
    array[item++] = new TestCase( SECTION,    "-0 / -1",                       0,               -0 / -1 );

    // Division of a non-zero finite value by a zero results in a signed infinity.

    array[item++] = new TestCase( SECTION,    "1 / 0",                          Number.POSITIVE_INFINITY,   1/0 );
    array[item++] = new TestCase( SECTION,    "1 / -0",                         Number.NEGATIVE_INFINITY,   1/-0 );
    array[item++] = new TestCase( SECTION,    "-1 / 0",                         Number.NEGATIVE_INFINITY,   -1/0 );
    array[item++] = new TestCase( SECTION,    "-1 / -0",                        Number.POSITIVE_INFINITY,   -1/-0 );

    array[item++] = new TestCase( SECTION,    "0 / Number.POSITIVE_INFINITY",   0,      0 / Number.POSITIVE_INFINITY );
    array[item++] = new TestCase( SECTION,    "0 / Number.NEGATIVE_INFINITY",   -0,     0 / Number.NEGATIVE_INFINITY );
    array[item++] = new TestCase( SECTION,    "-0 / Number.POSITIVE_INFINITY",  -0,     -0 / Number.POSITIVE_INFINITY );
    array[item++] = new TestCase( SECTION,    "-0 / Number.NEGATIVE_INFINITY",  0,      -0 / Number.NEGATIVE_INFINITY );

	// the sign of the result is positive if both operands have the same sign
	// the sign of the result is negative if the operands have different signs

    array[item++] = new TestCase( SECTION,    "1 / 1",                          1,              1 / 1 );
    array[item++] = new TestCase( SECTION,    "1 / -1",                        -1,              1 / -1 );
    array[item++] = new TestCase( SECTION,    "-1 / 1",                        -1,              -1 / 1 );
	var q = -1 / -1;
    array[item++] = new TestCase( SECTION,    "-1 / -1",                       1,               q );

	array[item++] = new TestCase( SECTION,    "Number.MIN_VALUE / Number.MAX_VALUE ",         0,   Number.MIN_VALUE / Number.MAX_VALUE );
	array[item++] = new TestCase( SECTION,    "Number.MAX_VALUE / Number.MIN_VALUE ",         Number.POSITIVE_INFINITY,   Number.MAX_VALUE / Number.MIN_VALUE );
	array[item++] = new TestCase( SECTION,    "10.3 / 5e-324",         Number.POSITIVE_INFINITY,   10.3 / 5e-324 );
	array[item++] = new TestCase( SECTION,    "Number.MIN_VALUE / 1.1 ",         4.9406564584124654e-324,   Number.MIN_VALUE / 1.1 );
	array[item++] = new TestCase( SECTION,    "Number.MIN_VALUE / 1.9 ",         4.9406564584124654e-324,   Number.MIN_VALUE / 1.9 );
	array[item++] = new TestCase( SECTION,    "Number.MIN_VALUE / 2.0 ",         0,   Number.MIN_VALUE / 2.0 );
	array[item++] = new TestCase( SECTION,    "Number.MIN_VALUE / 3 ",         0,   Number.MIN_VALUE / 3 );
	array[item++] = new TestCase( SECTION,    "Number.MIN_VALUE / 5 ",         0,   Number.MIN_VALUE / 5 );
	array[item++] = new TestCase( SECTION,    "Number.MIN_VALUE / 9 ",         0,   Number.MIN_VALUE / 9 );

    return ( array );
}
