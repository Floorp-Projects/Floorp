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

    var SECTION = "15.5.4.8-3";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "String.prototype.split";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    var TEST_STRING = "";
    var EXPECT = new Array();

    // this.toString is the empty string.

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String(); s.split().length",
                                    1,
                                    (s = new String(), s.split().length ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String(); s.split()[0]",
                                    "",
                                    (s = new String(), s.split()[0] ) );

    // this.toString() is the empty string, separator is specified.

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String(); s.split('').length",
                                    1,
                                    (s = new String(), s.split('').length ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String(); s.split(' ').length",
                                    1,
                                    (s = new String(), s.split(' ').length ) );

    // this to string is " "
    array[item++] = new TestCase(   SECTION,
                                    "var s = new String(' '); s.split().length",
                                    1,
                                    (s = new String(' '), s.split().length ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String(' '); s.split()[0]",
                                    " ",
                                    (s = new String(' '), s.split()[0] ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String(' '); s.split('').length",
                                    1,
                                    (s = new String(' '), s.split('').length ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String(' '); s.split('')[0]",
                                    " ",
                                    (s = new String(' '), s.split('')[0] ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String(' '); s.split(' ').length",
                                    2,
                                    (s = new String(' '), s.split(' ').length ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String(' '); s.split(' ')[0]",
                                    "",
                                    (s = new String(' '), s.split(' ')[0] ) );

    array[item++] = new TestCase(   SECTION,
                                    "\"\".split(\"\").length",
                                    1,
                                    ("".split("")).length );

    array[item++] = new TestCase(   SECTION,
                                    "\"\".split(\"x\").length",
                                    1,
                                    ("".split("x")).length );

    array[item++] = new TestCase(   SECTION,
                                    "\"\".split(\"x\")[0]",
                                    "",
                                    ("".split("x"))[0] );

    return array;
}

function Split( string, separator ) {
    string = String( string );

    var A = new Array();

    if ( arguments.length < 2 ) {
        A[0] = string;
        return A;
    }

    separator = String( separator );

    var str_len = String( string ).length;
    var sep_len = String( separator ).length;

    var p = 0;
    var k = 0;

    if ( sep_len == 0 ) {
        for ( ; p < str_len; p++ ) {
            A[A.length] = String( string.charAt(p) );
        }
    }
    return A;
}
