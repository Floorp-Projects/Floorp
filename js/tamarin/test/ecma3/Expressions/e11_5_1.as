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
    var SECTION = "e11_5_1";
    var VERSION = "ECMA_1";
    startTest();
    var testcases = getTestCases();

    writeHeaderToLog( SECTION + " Applying the * operator");
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION,    "Number.NaN * Number.NaN",    Number.NaN,     Number.NaN * Number.NaN );
    array[item++] = new TestCase( SECTION,    "Number.NaN * 1",             Number.NaN,     Number.NaN * 1 );
    array[item++] = new TestCase( SECTION,    "1 * Number.NaN",             Number.NaN,     1 * Number.NaN );

    array[item++] = new TestCase( SECTION,    "Number.POSITIVE_INFINITY * 0",   Number.NaN, Number.POSITIVE_INFINITY * 0 );
    array[item++] = new TestCase( SECTION,    "Number.NEGATIVE_INFINITY * 0",   Number.NaN, Number.NEGATIVE_INFINITY * 0 );
    array[item++] = new TestCase( SECTION,    "0 * Number.POSITIVE_INFINITY",   Number.NaN, 0 * Number.POSITIVE_INFINITY );
    array[item++] = new TestCase( SECTION,    "0 * Number.NEGATIVE_INFINITY",   Number.NaN, 0 * Number.NEGATIVE_INFINITY );

    array[item++] = new TestCase( SECTION,    "-0 * Number.POSITIVE_INFINITY",  Number.NaN,   -0 * Number.POSITIVE_INFINITY );
    array[item++] = new TestCase( SECTION,    "-0 * Number.NEGATIVE_INFINITY",  Number.NaN,   -0 * Number.NEGATIVE_INFINITY );
    array[item++] = new TestCase( SECTION,    "Number.POSITIVE_INFINITY * -0",  Number.NaN,   Number.POSITIVE_INFINITY * -0 );
    array[item++] = new TestCase( SECTION,    "Number.NEGATIVE_INFINITY * -0",  Number.NaN,   Number.NEGATIVE_INFINITY * -0 );

    array[item++] = new TestCase( SECTION,    "0 * -0",                         -0,         0 * -0 );
    array[item++] = new TestCase( SECTION,    "-0 * 0",                         -0,         -0 * 0 );
    array[item++] = new TestCase( SECTION,    "-0 * -0",                        0,          -0 * -0 );
    array[item++] = new TestCase( SECTION,    "0 * 0",                          0,          0 * 0 );

    array[item++] = new TestCase( SECTION,    "Number.NEGATIVE_INFINITY * Number.NEGATIVE_INFINITY",    Number.POSITIVE_INFINITY,   Number.NEGATIVE_INFINITY * Number.NEGATIVE_INFINITY );
    array[item++] = new TestCase( SECTION,    "Number.POSITIVE_INFINITY * Number.NEGATIVE_INFINITY",    Number.NEGATIVE_INFINITY,   Number.POSITIVE_INFINITY * Number.NEGATIVE_INFINITY );
    array[item++] = new TestCase( SECTION,    "Number.NEGATIVE_INFINITY * Number.POSITIVE_INFINITY",    Number.NEGATIVE_INFINITY,   Number.NEGATIVE_INFINITY * Number.POSITIVE_INFINITY );
    array[item++] = new TestCase( SECTION,    "Number.POSITIVE_INFINITY * Number.POSITIVE_INFINITY",    Number.POSITIVE_INFINITY,   Number.POSITIVE_INFINITY * Number.POSITIVE_INFINITY );

    array[item++] = new TestCase( SECTION,    "Number.NEGATIVE_INFINITY * 1 ",                          Number.NEGATIVE_INFINITY,   Number.NEGATIVE_INFINITY * 1 );
    array[item++] = new TestCase( SECTION,    "Number.NEGATIVE_INFINITY * -1 ",                         Number.POSITIVE_INFINITY,   Number.NEGATIVE_INFINITY * -1 );
    array[item++] = new TestCase( SECTION,    "1 * Number.NEGATIVE_INFINITY",                           Number.NEGATIVE_INFINITY,   1 * Number.NEGATIVE_INFINITY );
    array[item++] = new TestCase( SECTION,    "-1 * Number.NEGATIVE_INFINITY",                          Number.POSITIVE_INFINITY,   -1 * Number.NEGATIVE_INFINITY );

    array[item++] = new TestCase( SECTION,    "Number.POSITIVE_INFINITY * 1 ",                          Number.POSITIVE_INFINITY,   Number.POSITIVE_INFINITY * 1 );
    array[item++] = new TestCase( SECTION,    "Number.POSITIVE_INFINITY * -1 ",                         Number.NEGATIVE_INFINITY,   Number.POSITIVE_INFINITY * -1 );
    array[item++] = new TestCase( SECTION,    "1 * Number.POSITIVE_INFINITY",                           Number.POSITIVE_INFINITY,   1 * Number.POSITIVE_INFINITY );
	array[item++] = new TestCase( SECTION,    "-1 * Number.POSITIVE_INFINITY",                          Number.NEGATIVE_INFINITY,   -1 * Number.POSITIVE_INFINITY );

	array[item++] = new TestCase( SECTION,    "-1.0 * Number.MAX_VALUE",                         -Number.MAX_VALUE,     -1.0 * Number.MAX_VALUE );
	array[item++] = new TestCase( SECTION,    "1.1 * Number.MAX_VALUE",                          Number.POSITIVE_INFINITY,   1.1 * Number.MAX_VALUE );
	array[item++] = new TestCase( SECTION,    "-1.1 * Number.MAX_VALUE",                         Number.NEGATIVE_INFINITY,   -1.1 * Number.MAX_VALUE );
	array[item++] = new TestCase( SECTION,    "Number.MAX_VALUE * Number.MAX_VALUE",             Number.POSITIVE_INFINITY,   Number.MAX_VALUE * Number.MAX_VALUE );
	array[item++] = new TestCase( SECTION,    "(Number.MAX_VALUE-1) * (Number.MAX_VALUE-1)",     Number.POSITIVE_INFINITY,   (Number.MAX_VALUE-1) * (Number.MAX_VALUE-1) );
	array[item++] = new TestCase( SECTION,    "-1 * Number.MAX_VALUE * Number.MAX_VALUE",        Number.NEGATIVE_INFINITY,   -1 * Number.MAX_VALUE * Number.MAX_VALUE );

	array[item++] = new TestCase( SECTION,    "-1 * Number.MIN_VALUE",                           -4.9406564584124654e-324,   -1 * Number.MIN_VALUE );
	array[item++] = new TestCase( SECTION,    "0.9 * Number.MIN_VALUE",                          4.9406564584124654e-324,	 0.9 * Number.MIN_VALUE );
	array[item++] = new TestCase( SECTION,    "-0.9 * Number.MIN_VALUE",                         -4.9406564584124654e-324, -0.9 * Number.MIN_VALUE );
	array[item++] = new TestCase( SECTION,    "0.51 * Number.MIN_VALUE",                          4.9406564584124654e-324,	 0.51 * Number.MIN_VALUE );
	array[item++] = new TestCase( SECTION,    "-0.51 * Number.MIN_VALUE",                         -4.9406564584124654e-324,	-0.51 * Number.MIN_VALUE );
	array[item++] = new TestCase( SECTION,    "0.5 * Number.MIN_VALUE",                          0,   						 0.5 * Number.MIN_VALUE );
	array[item++] = new TestCase( SECTION,    "-0.5 * Number.MIN_VALUE",                         -0,   						 -0.5 * Number.MIN_VALUE );
	array[item++] = new TestCase( SECTION,    "0.1 * Number.MIN_VALUE",                          0,   						 0.1 * Number.MIN_VALUE );
	array[item++] = new TestCase( SECTION,    "-0.1 * Number.MIN_VALUE",                         -0,   						 -0.1 * Number.MIN_VALUE );
	array[item++] = new TestCase( SECTION,    "Number.MIN_VALUE * Number.MIN_VALUE",             0,   						 Number.MIN_VALUE * Number.MIN_VALUE );

	array[item++] = new TestCase( SECTION,    "1 * 1",        1,   	  1 * 1 );
	array[item++] = new TestCase( SECTION,    "-1 * 1",      -1,   	 -1 * 1 );
	array[item++] = new TestCase( SECTION,    "1 * (-1)",    -1,   	  1 * (-1) );
	array[item++] = new TestCase( SECTION,    "-1 * (-1)",    1,   	 -1 * (-1) );
	
	

    return ( array );
}
