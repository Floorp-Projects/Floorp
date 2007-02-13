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
    var SECTION = "e11_4_2";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "The void operator";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();

    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION,   "void(new String('string object'))",      void 0,  void(new String( 'string object' )) );
    array[item++] = new TestCase( SECTION,   "void('string primitive')",               void 0,  void("string primitive") );
    array[item++] = new TestCase( SECTION,   "void(Number.NaN)",                       void 0,  void(Number.NaN) );
    array[item++] = new TestCase( SECTION,   "void(Number.POSITIVE_INFINITY)",         void 0,  void(Number.POSITIVE_INFINITY) );
    array[item++] = new TestCase( SECTION,   "void(1)",                                void 0,  void(1) );
    array[item++] = new TestCase( SECTION,   "void(0)",                                void 0,  void(0) );
    array[item++] = new TestCase( SECTION,   "void(-1)",                               void 0,  void(-1) );
    array[item++] = new TestCase( SECTION,   "void(Number.NEGATIVE_INFINITY)",         void 0,  void(Number.NEGATIVE_INFINITY) );
    array[item++] = new TestCase( SECTION,   "void(Math.PI)",                          void 0,  void(Math.PI) );
    array[item++] = new TestCase( SECTION,   "void(true)",                             void 0,  void(true) );
    array[item++] = new TestCase( SECTION,   "void(false)",                            void 0,  void(false) );
    array[item++] = new TestCase( SECTION,   "void(null)",                             void 0,  void(null) );
    array[item++] = new TestCase( SECTION,   "void new String('string object')",      void 0,  void new String( 'string object' ) );
    array[item++] = new TestCase( SECTION,   "void 'string primitive'",               void 0,  void "string primitive" );
    array[item++] = new TestCase( SECTION,   "void Number.NaN",                       void 0,  void Number.NaN );
    array[item++] = new TestCase( SECTION,   "void Number.POSITIVE_INFINITY",         void 0,  void Number.POSITIVE_INFINITY );
    array[item++] = new TestCase( SECTION,   "void 1",                                void 0,  void 1 );
    array[item++] = new TestCase( SECTION,   "void 0",                                void 0,  void 0 );
    array[item++] = new TestCase( SECTION,   "void -1",                               void 0,  void -1 );
    array[item++] = new TestCase( SECTION,   "void Number.NEGATIVE_INFINITY",         void 0,  void Number.NEGATIVE_INFINITY );
    array[item++] = new TestCase( SECTION,   "void Math.PI",                          void 0,  void Math.PI );
    array[item++] = new TestCase( SECTION,   "void true",                             void 0,  void true );
    array[item++] = new TestCase( SECTION,   "void false",                            void 0,  void false );
    array[item++] = new TestCase( SECTION,   "void null",                             void 0,  void null );

//     array[item++] = new TestCase( SECTION,   "void()",                                 void 0,  void() );

    return ( array );
}

function test() {
    for ( i = 0; i < testcases.length; i++ ) {
            testcases[i].passed = writeTestCaseResult(
                    testcases[i].expect,
                    testcases[i].actual,
                    testcases[i].description +" = "+ testcases[i].actual );
            testcases[i].reason += ( testcases[i].passed ) ? "" : "wrong value "
    }
    stopTest();
    return ( testcases );
}
