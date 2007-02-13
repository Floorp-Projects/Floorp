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
    var SECTION = "RegExp/function-001";
    var VERSION = "ECMA_2";
    var TITLE   = "RegExp( pattern, flags )";

	startTest();
	writeHeaderToLog(SECTION + " " + TITLE);
	var testcases = getTestCases();
	test();

function getTestCases() {
	var array = new Array();
	var item = 0;

    /*
     * for each test case, verify:
     * - verify that [[Class]] property is RegExp
     * - prototype property should be set to RegExp.prototype
     * - source is set to the empty string
     * - global property is set to false
     * - ignoreCase property is set to false
     * - multiline property is set to false
     * - lastIndex property is set to 0
     */

    RegExp.prototype.getClassProperty = Object.prototype.toString;
    var re = new RegExp();

    /*array[item++] = new TestCase(SECTION,
        "new RegExp().constructor.prototype",
        RegExp.prototype,
        re.constructor.prototype
    );*/

    array[item++] = new TestCase(SECTION,
        "RegExp.prototype.getClassProperty = Object.prototype.toString; " +
        "(new RegExp()).getClassProperty()",
        "[object RegExp]",
        re.getClassProperty() );

    array[item++] = new TestCase(SECTION,
        "(new RegExp()).source",
        "",
        re.source );

    array[item++] = new TestCase(SECTION,
        "(new RegExp()).global",
        false,
        re.global );

    array[item++] = new TestCase(SECTION,
        "(new RegExp()).ignoreCase",
        false,
        re.ignoreCase );

    array[item++] = new TestCase(SECTION,
        "(new RegExp()).multiline",
        false,
        re.multiline );

    array[item++] = new TestCase(SECTION,
        "(new RegExp()).lastIndex",
        0,
        re.lastIndex );
        
    //restore
    delete RegExp.prototype.getClassProperty;
    
	return array;
}
