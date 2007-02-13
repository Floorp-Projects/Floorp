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
    var SECTION = "9.3";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "ToNumber";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    // special cases here

    array[item++] = new TestCase( SECTION,   "Number()",                      0,              Number() );
    //array[item++] = new TestCase( SECTION,   "Number(eval('var x'))",         Number.NaN,     Number(eval("var x")) );
    var x;
    array[item++] = new TestCase( SECTION,   "Number(var x)",         Number.NaN,     Number(x) );
    array[item++] = new TestCase( SECTION,   "Number(void 0)",                Number.NaN,     Number(void 0) );
    array[item++] = new TestCase( SECTION,   "Number(null)",                  0,              Number(null) );
    array[item++] = new TestCase( SECTION,   "Number(true)",                  1,              Number(true) );
    array[item++] = new TestCase( SECTION,   "Number(false)",                 0,              Number(false) );
    array[item++] = new TestCase( SECTION,   "Number(0)",                     0,              Number(0) );
    array[item++] = new TestCase( SECTION,   "Number(-0)",                    -0,             Number(-0) );
    array[item++] = new TestCase( SECTION,   "Number(1)",                     1,              Number(1) );
    array[item++] = new TestCase( SECTION,   "Number(-1)",                    -1,             Number(-1) );
    array[item++] = new TestCase( SECTION,   "Number(Number.MAX_VALUE)",      1.7976931348623157e308, Number(Number.MAX_VALUE) );
    array[item++] = new TestCase( SECTION,   "Number(Number.MIN_VALUE)",      5e-324,         Number(Number.MIN_VALUE) );

    array[item++] = new TestCase( SECTION,   "Number(Number.NaN)",                Number.NaN,                 Number(Number.NaN) );
    array[item++] = new TestCase( SECTION,   "Number(Number.POSITIVE_INFINITY)",  Number.POSITIVE_INFINITY,   Number(Number.POSITIVE_INFINITY) );
    array[item++] = new TestCase( SECTION,   "Number(Number.NEGATIVE_INFINITY)",  Number.NEGATIVE_INFINITY,   Number(Number.NEGATIVE_INFINITY) );

    return ( array );
}
