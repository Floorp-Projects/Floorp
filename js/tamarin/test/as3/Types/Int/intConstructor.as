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
var SECTION = "as3";
var VERSION = "as3";
startTest();
var TITLE   = "The int Constructor Called as a Function";

writeHeaderToLog( SECTION + " "+ TITLE);

var testcases = new Array();

testcases = getTestCases();

test();


function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase(SECTION, "int()",                  0,          int() );
    array[item++] = new TestCase(SECTION, "int(void 0)",            0,  int(void 0) );
    array[item++] = new TestCase(SECTION, "int(null)",              0,          int(null) );
    array[item++] = new TestCase(SECTION, "int()",                  0,          int() );
    array[item++] = new TestCase(SECTION, "int(new int())",      0,          int( new int() ) );
    array[item++] = new TestCase(SECTION, "int(0)",                 0,          int(0) );
    array[item++] = new TestCase(SECTION, "int(1)",                 1,          int(1) );
    array[item++] = new TestCase(SECTION, "int(-1)",                -1,         int(-1) );
    array[item++] = new TestCase(SECTION, "int('string')",          0, int( "string") );
    array[item++] = new TestCase(SECTION, "int(new String())",      0,          int( new String() ) );
    array[item++] = new TestCase(SECTION, "int('')",                0,          int( "" ) );

    array[item++] = new TestCase(SECTION, "int(new MyObject(100))",  100,        int(new MyObject(100)) );

    return ( array );
}
/*function test() {
    for ( tc=0; tc < testcases.length; tc++ ) {
        testcases[tc].passed = writeTestCaseResult(
                            testcases[tc].expect,
                            testcases[tc].actual,
                            testcases[tc].description +" = "+
                            testcases[tc].actual );

        testcases[tc].reason += ( testcases[tc].passed ) ? "" : "wrong value ";
    }
    stopTest();
    return ( testcases );
}*/
function MyObject( value ) {
    this.value = value;
    this.valueOf = function() { return this.value; }
}
