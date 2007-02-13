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
    var SECTION = "15.4.2.1-3";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "The Array Constructor:  new Array( item0, item1, ...)";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();

    var ARGUMENTS = ""
    var TEST_LENGTH = Math.pow(2,10); //Math.pow(2,32);

    for ( var index = 0; index < TEST_LENGTH; index++ ) {
        ARGUMENTS += index;
        ARGUMENTS += (index == (TEST_LENGTH-1) ) ? "" : ",";
    }

    var TEST_ARRAY = ARGUMENTS.split(",");

	var item;
    for ( item = 0; item < TEST_LENGTH; item++ ) {
        array[item] = new TestCase( SECTION, "TEST_ARRAY["+item+"]",     item+"",    TEST_ARRAY[item] );
    }

    array[item++] = new TestCase( SECTION, "new Array( ["+TEST_LENGTH+" arguments] ) +''",  ARGUMENTS,          TEST_ARRAY +"" );
    array[item++] = new TestCase( SECTION, "TEST_ARRAY.toString", "function Function() {}", (TEST_ARRAY.toString).toString());
    array[item++] = new TestCase( SECTION, "TEST_ARRAY.join", "function Function() {}", (TEST_ARRAY.join).toString() );
    array[item++] = new TestCase( SECTION, "TEST_ARRAY.sort", "function Function() {}", (TEST_ARRAY.sort).toString() );
    array[item++] = new TestCase( SECTION, "TEST_ARRAY.reverse", "function Function() {}", (TEST_ARRAY.reverse).toString());
    array[item++] = new TestCase( SECTION, "TEST_ARRAY.length", TEST_LENGTH,  TEST_ARRAY.length);

	TEST_ARRAY.toString = Object.prototype.toString;
	array[item++] = new TestCase( SECTION,
                                "TEST_ARRAY.toString = Object.prototype.toString; TEST_ARRAY.toString()",
                                "[object Array]",
                                TEST_ARRAY.toString());

    return ( array );
}
