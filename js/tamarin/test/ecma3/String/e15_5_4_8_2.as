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

    var SECTION = "15.5.4.8-2";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "String.prototype.split";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    // case where separator is the empty string.

    var TEST_STRING = "this is a string object";

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String( "+ TEST_STRING +" ); s.split('').length",
                                    TEST_STRING.length,
                                    (s = new String( TEST_STRING ), s.split('').length ) );

    for ( var i = 0; i < TEST_STRING.length; i++ ) {

        array[item++] = new TestCase(   SECTION,
                                    "var s = new String( "+TEST_STRING+" ); s.split('')["+i+"]",
                                    TEST_STRING.charAt(i),
                                    (s = new String( TEST_STRING ), s.split('')[i]) );
    }

    // case where the value of the separator is undefined.  in this case. the value of the separator
    // should be ToString( separator ), or "undefined".

    var TEST_STRING = "thisundefinedisundefinedaundefinedstringundefinedobject";
    var EXPECT_STRING = new Array( "this", "is", "a", "string", "object" );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String( "+ TEST_STRING +" ); s.split(void 0).length",
                                    EXPECT_STRING.length,
                                    (s = new String( TEST_STRING ), s.split(void 0).length ) );

    for ( var i = 0; i < EXPECT_STRING.length; i++ ) {
        array[item++] = new TestCase(   SECTION,
                                    "var s = new String( "+TEST_STRING+" ); s.split(void 0)["+i+"]",
                                    EXPECT_STRING[i],
                                    (s = new String( TEST_STRING ), s.split(void 0)[i]) );
    }

    // case where the value of the separator is null.  in this case the value of the separator is "null".
    TEST_STRING = "thisnullisnullanullstringnullobject";
    var EXPECT_STRING = new Array( "this", "is", "a", "string", "object" );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String( "+ TEST_STRING +" ); s.split(null).length",
                                    EXPECT_STRING.length,
                                    (s = new String( TEST_STRING ), s.split(null).length ) );

    for ( var i = 0; i < EXPECT_STRING.length; i++ ) {
        array[item++] = new TestCase(   SECTION,
                                    "var s = new String( "+TEST_STRING+" ); s.split(null)["+i+"]",
                                    EXPECT_STRING[i],
                                    (s = new String( TEST_STRING ), s.split(null)[i]) );
    }

    // case where the value of the separator is a boolean.
    TEST_STRING = "thistrueistrueatruestringtrueobject";
    var EXPECT_STRING = new Array( "this", "is", "a", "string", "object" );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String( "+ TEST_STRING +" ); s.split(true).length",
                                    EXPECT_STRING.length,
                                    (s = new String( TEST_STRING ), s.split(true).length ) );

    for ( var i = 0; i < EXPECT_STRING.length; i++ ) {
        array[item++] = new TestCase(   SECTION,
                                    "var s = new String( "+TEST_STRING+" ); s.split(true)["+i+"]",
                                    EXPECT_STRING[i],
                                    (s = new String( TEST_STRING ), s.split(true)[i]) );
    }

    // case where the value of the separator is a number
    TEST_STRING = "this123is123a123string123object";
    var EXPECT_STRING = new Array( "this", "is", "a", "string", "object" );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String( "+ TEST_STRING +" ); s.split(123).length",
                                    EXPECT_STRING.length,
                                    (s = new String( TEST_STRING ), s.split(123).length ) );

    for ( var i = 0; i < EXPECT_STRING.length; i++ ) {
        array[item++] = new TestCase(   SECTION,
                                    "var s = new String( "+TEST_STRING+" ); s.split(123)["+i+"]",
                                    EXPECT_STRING[i],
                                    (s = new String( TEST_STRING ), s.split(123)[i]) );
    }


    // case where the value of the separator is a number
    TEST_STRING = "this123is123a123string123object";
    var EXPECT_STRING = new Array( "this", "is", "a", "string", "object" );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String( "+ TEST_STRING +" ); s.split(123).length",
                                    EXPECT_STRING.length,
                                    (s = new String( TEST_STRING ), s.split(123).length ) );

    for ( var i = 0; i < EXPECT_STRING.length; i++ ) {
        array[item++] = new TestCase(   SECTION,
                                    "var s = new String( "+TEST_STRING+" ); s.split(123)["+i+"]",
                                    EXPECT_STRING[i],
                                    (s = new String( TEST_STRING ), s.split(123)[i]) );
    }

    // case where the separator is not in the string
    TEST_STRING = "this is a string";
    EXPECT_STRING = new Array( "this is a string" );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String( " + TEST_STRING + " ); s.split(':').length",
                                    1,
                                    (s = new String( TEST_STRING ), s.split(':').length ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String( " + TEST_STRING + " ); s.split(':')[0]",
                                    TEST_STRING,
                                    (s = new String( TEST_STRING ), s.split(':')[0] ) );

    // case where part but not all of separator is in the string.
    TEST_STRING = "this is a string";
    EXPECT_STRING = new Array( "this is a string" );
    array[item++] = new TestCase(   SECTION,
                                    "var s = new String( " + TEST_STRING + " ); s.split('strings').length",
                                    1,
                                    (s = new String( TEST_STRING ), s.split('strings').length ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String( " + TEST_STRING + " ); s.split('strings')[0]",
                                    TEST_STRING,
                                    (s = new String( TEST_STRING ), s.split('strings')[0] ) );

    // case where the separator is at the end of the string
    TEST_STRING = "this is a string";
    EXPECT_STRING = new Array( "this is a " );
    array[item++] = new TestCase(   SECTION,
                                    "var s = new String( " + TEST_STRING + " ); s.split('string').length",
                                    2,
                                    (s = new String( TEST_STRING ), s.split('string').length ) );

    for ( var i = 0; i < EXPECT_STRING.length; i++ ) {
        array[item++] = new TestCase(   SECTION,
                                    "var s = new String( "+TEST_STRING+" ); s.split('string')["+i+"]",
                                    EXPECT_STRING[i],
                                    (s = new String( TEST_STRING ), s.split('string')[i]) );
    }
    return array;
}

