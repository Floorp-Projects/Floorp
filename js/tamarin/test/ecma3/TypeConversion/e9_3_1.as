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
    var SECTION = "9.3-1";
    var VERSION = "ECMA_1";
    startTest();
    var TYPE = "number";
    var testcases = getTestCases();

    writeHeaderToLog( SECTION + " ToNumber");
    test();

function test() {
    for ( tc=0; tc < testcases.length; tc++ ) {
        testcases[tc].passed = writeTestCaseResult(
                            testcases[tc].expect,
                            testcases[tc].actual,
                            testcases[tc].description +" = "+
                            testcases[tc].actual );

        testcases[tc].passed = writeTestCaseResult(
                                TYPE,
                                typeof(testcases[tc].actual),
                                "typeof( " + testcases[tc].description +
                                " ) = " + typeof(testcases[tc].actual) )
                                ? testcases[tc].passed
                                : false;

        testcases[tc].reason += ( testcases[tc].passed ) ? "" : "wrong value ";
    }
    stopTest();
    return ( testcases );
}
function getTestCases() {
    var array = new Array();
    var item = 0;

    // object is Number
    array[item++] = new TestCase( SECTION,   "Number(new Number())",          0,              Number(new Number())  );
    array[item++] = new TestCase( SECTION,   "Number(new Number(Number.NaN))",Number.NaN,     Number(new Number(Number.NaN)) );
    array[item++] = new TestCase( SECTION,   "Number(new Number(0))",         0,              Number(new Number(0)) );
    array[item++] = new TestCase( SECTION,   "Number(new Number(null))",      0,              Number(new Number(null)) );
//    array[item++] = new TestCase( SECTION,   "Number(new Number(void 0))",    Number.NaN,     Number(new Number(void 0)) );
    array[item++] = new TestCase( SECTION,   "Number(new Number(true))",      1,              Number(new Number(true)) );
    array[item++] = new TestCase( SECTION,   "Number(new Number(false))",     0,              Number(new Number(false)) );

    // object is boolean

    array[item++] = new TestCase( SECTION,   "Number(new Boolean(true))",     1,  Number(new Boolean(true)) );
    array[item++] = new TestCase( SECTION,   "Number(new Boolean(false))",    0,  Number(new Boolean(false)) );

    // object is array
    array[item++] = new TestCase( SECTION,   "Number(new Array(2,4,8,16,32))",      Number.NaN,     Number(new Array(2,4,8,16,32)) );

    return ( array );
}
