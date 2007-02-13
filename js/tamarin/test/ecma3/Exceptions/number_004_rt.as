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
    var SECTION = "number-003";
    var VERSION = "ECMA_4";
    var TITLE   = "Exceptions for Number.toString()";

    startTest();
    writeHeaderToLog( SECTION + " Number.prototype.toString()");

    var testcases = getTestCases();
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;  


    var result = "Failed";
    var exception = "No exception thrown";
    var expect = "Passed";

    try {
        tostr = Number.prototype.toString;
        OBJECT = new String("Infinity");
        OBJECT.toString= tostr;
        result = OBJECT.toString();
    } catch ( e:ReferenceError ) {
        result = expect;
        exception = e.toString();
    }

    array[item++] = new TestCase(
        SECTION,
        "Assigning Number.prototype.toString as the toString of a String object " +
        " (threw " + referenceError(exception) +")",
        expect,
        result );

	// new Number()
    try {
        tostr= Number.prototype.toString;
        OBJECT = new Number();
        OBJECT.toString= tostr;
        result = OBJECT.toString();
    } catch ( e1:ReferenceError ) {
        result = expect;
        exception = e1.toString();
    }

    array[item++] = new TestCase(
        SECTION,
        "Assigning Number.prototype.toString as the toString of new Number() " +
        " (threw " + referenceError(exception) +")",
        expect,
        result );

	// new Number(4)
    try {
        tostr= Number.prototype.toString;
        OBJECT = new Number(4);
        OBJECT.toString= tostr;
        result = OBJECT.toString ();
    } catch ( e2:ReferenceError ) {
        result = expect;
        exception = e2.toString();
    }

    array[item++] = new TestCase(
        SECTION,
        "Assigning Number.prototype.toString as the toString of new Number(4) " +
        " (threw " + referenceError(exception) +")",
        expect,
        result );

	// 4
    try {
        tostr= Number.prototype.toString;
        OBJECT = 4;
        OBJECT.toString= tostr;
        result = OBJECT.toString();
    } catch ( e3:ReferenceError ) {
        result = expect;
        exception = e3.toString();
    }

    array[item++] = new TestCase(
        SECTION,
        "Assigning Number.prototype.toString as the toString of '4' " +
        " (threw " + referenceError(exception) +")",
        expect,
        result );

    return array;
}

