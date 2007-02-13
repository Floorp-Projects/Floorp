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
    var SECTION = "e11_6_3";
    var VERSION = "ECMA_1";
    startTest();
    var testcases = getTestCases();

    writeHeaderToLog( SECTION + " Applying the additive operators (+,-) to numbers");
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION,    "Number.NaN + 1",     Number.NaN,     Number.NaN + 1 );
    array[item++] = new TestCase( SECTION,    "1 + Number.NaN",     Number.NaN,     1 + Number.NaN );

    array[item++] = new TestCase( SECTION,    "Number.NaN - 1",     Number.NaN,     Number.NaN - 1 );
    array[item++] = new TestCase( SECTION,    "1 - Number.NaN",     Number.NaN,     1 - Number.NaN );

    array[item++] = new TestCase( SECTION,  "Number.POSITIVE_INFINITY + Number.POSITIVE_INFINITY",  Number.POSITIVE_INFINITY,   Number.POSITIVE_INFINITY + Number.POSITIVE_INFINITY);
    array[item++] = new TestCase( SECTION,  "Number.NEGATIVE_INFINITY + Number.NEGATIVE_INFINITY",  Number.NEGATIVE_INFINITY,   Number.NEGATIVE_INFINITY + Number.NEGATIVE_INFINITY);

    array[item++] = new TestCase( SECTION,  "Number.POSITIVE_INFINITY + Number.NEGATIVE_INFINITY",  Number.NaN,     Number.POSITIVE_INFINITY + Number.NEGATIVE_INFINITY);
    array[item++] = new TestCase( SECTION,  "Number.NEGATIVE_INFINITY + Number.POSITIVE_INFINITY",  Number.NaN,     Number.NEGATIVE_INFINITY + Number.POSITIVE_INFINITY);

    array[item++] = new TestCase( SECTION,  "Number.POSITIVE_INFINITY - Number.POSITIVE_INFINITY",  Number.NaN,   Number.POSITIVE_INFINITY - Number.POSITIVE_INFINITY);
    array[item++] = new TestCase( SECTION,  "Number.NEGATIVE_INFINITY - Number.NEGATIVE_INFINITY",  Number.NaN,   Number.NEGATIVE_INFINITY - Number.NEGATIVE_INFINITY);

    array[item++] = new TestCase( SECTION,  "Number.POSITIVE_INFINITY - Number.NEGATIVE_INFINITY",  Number.POSITIVE_INFINITY,   Number.POSITIVE_INFINITY - Number.NEGATIVE_INFINITY);
    array[item++] = new TestCase( SECTION,  "Number.NEGATIVE_INFINITY - Number.POSITIVE_INFINITY",  Number.NEGATIVE_INFINITY,   Number.NEGATIVE_INFINITY - Number.POSITIVE_INFINITY);

    array[item++] = new TestCase( SECTION,  "-0 + -0",      -0,     -0 + -0 );
    array[item++] = new TestCase( SECTION,  "-0 - 0",       -0,     -0 - 0 );

    array[item++] = new TestCase( SECTION,  "0 + 0",        0,      0 + 0 );
    array[item++] = new TestCase( SECTION,  "0 + -0",       0,      0 + -0 );
    array[item++] = new TestCase( SECTION,  "0 - -0",       0,      0 - -0 );
    array[item++] = new TestCase( SECTION,  "0 - 0",        0,      0 - 0 );
    array[item++] = new TestCase( SECTION,  "-0 - -0",      0,     -0 - -0 );
    array[item++] = new TestCase( SECTION,  "-0 + 0",       0,     -0 + 0 );

    array[item++] = new TestCase( SECTION,  "Number.MAX_VALUE - Number.MAX_VALUE",      0,  Number.MAX_VALUE - Number.MAX_VALUE );
    array[item++] = new TestCase( SECTION,  "1/Number.MAX_VALUE - 1/Number.MAX_VALUE",  0,  1/Number.MAX_VALUE - 1/Number.MAX_VALUE );

    array[item++] = new TestCase( SECTION,  "Number.MIN_VALUE - Number.MIN_VALUE",      0,  Number.MIN_VALUE - Number.MIN_VALUE );

// the sum of an infinity and a finite value is equal to the infinite operand
    array[item++] = new TestCase( SECTION,  "Number.POSITIVE_INFINITY + 543.87",  Number.POSITIVE_INFINITY,   Number.POSITIVE_INFINITY + 543.87);
    array[item++] = new TestCase( SECTION,  "Number.NEGATIVE_INFINITY + 87456.093",  Number.NEGATIVE_INFINITY,   Number.NEGATIVE_INFINITY + 87456.093);
    array[item++] = new TestCase( SECTION,  "95665 + Number.POSITIVE_INFINITY ",  Number.POSITIVE_INFINITY,   95665 + Number.POSITIVE_INFINITY);
    array[item++] = new TestCase( SECTION,  "32.543906 + Number.NEGATIVE_INFINITY ",  Number.NEGATIVE_INFINITY,   32.543906 + Number.NEGATIVE_INFINITY);

// the sum of a zero and a nonzero finite value is equal to the nonzero operand
    array[item++] = new TestCase( SECTION,  "0 + 40453.65",    40453.65,      0 + 40453.65 );
    array[item++] = new TestCase( SECTION,  "0 - 745.33",       -745.33,      0  - 745.33);
    array[item++] = new TestCase( SECTION,  "67007.5 + 0",     67007.5 ,      67007.5  + 0 );
    array[item++] = new TestCase( SECTION,  "2480.00 - 0",     2480.00 ,      2480.00 - 0 );

// the sum of two nonzero finite values of the same magnitude and opposite sign is +0	
    array[item++] = new TestCase( SECTION,  "2480.00 + -2480.00",     +0 ,      2480.00 + -2480.00 );
    array[item++] = new TestCase( SECTION,  "-268.05 + 268.05",     +0 ,     -268.05 + 268.05 );
	
    array[item++] = new TestCase( SECTION,  "Number.MAX_VALUE + 1",     "1.79769313486231e+308",  Number.MAX_VALUE + 1+"");
    array[item++] = new TestCase( SECTION,  "Number.MAX_VALUE + 99.99",     "1.79769313486231e+308",  Number.MAX_VALUE + 99.99+"" );
    array[item++] = new TestCase( SECTION,  "4324.43 + (-64.000503)",      4260.429497, 4324.43 + (-64.000503) );
	
	
    return ( array );
}
