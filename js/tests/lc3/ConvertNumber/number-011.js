/* The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation. All Rights Reserved.
 * 
 */
/**
 *  Preferred Argument Conversion.
 *
 *  Use the explicit method invocation syntax to override the preferred
 *  argument conversion.
 *
 */
    var SECTION = "Preferred argument conversion:  undefined";
    var VERSION = "1_4";
    var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
                    SECTION;
    startTest();

    var TEST_CLASS = new
        Packages.com.netscape.javascript.qa.lc3.number.Number_001;

    testcases[testcases.length] = new TestCase(
        "TEST_CLASS[\"ambiguous(java.lang.Object)\"](1)",
        "OBJECT",
        TEST_CLASS["ambiguous(java.lang.Object)"](1) +'');


    testcases[testcases.length] = new TestCase(
        "TEST_CLASS[\"ambiguous(java.lang.String)\"](1)",
        "STRING",
        TEST_CLASS["ambiguous(java.lang.String)"](1) +'');

    testcases[testcases.length] = new TestCase(
        "TEST_CLASS[\"ambiguous(byte)\"](1)",
        "BYTE",
        TEST_CLASS["ambiguous(byte)"](1) +'');

    testcases[testcases.length] = new TestCase(
        "TEST_CLASS[\"ambiguous(char)\"](1)",
        "CHAR",
        TEST_CLASS["ambiguous(char)"](1) +'');

    testcases[testcases.length] = new TestCase(
        "TEST_CLASS[\"ambiguous(short)\"](1)",
        "SHORT",
        TEST_CLASS["ambiguous(short)"](1) +'');

    testcases[testcases.length] = new TestCase(
        "TEST_CLASS[\"ambiguous(int)\"](1)",
        "INT",
        TEST_CLASS["ambiguous(int)"](1) +'');

    testcases[testcases.length] = new TestCase(
        "TEST_CLASS[\"ambiguous(long)\"](1)",
        "LONG",
        TEST_CLASS["ambiguous(long)"](1) +'');

    testcases[testcases.length] = new TestCase(
        "TEST_CLASS[\"ambiguous(float)\"](1)",
        "FLOAT",
        TEST_CLASS["ambiguous(float)"](1) +'');

    testcases[testcases.length] = new TestCase(
        "TEST_CLASS[\"ambiguous(java.lang.Double)\"](1)",
        "DOUBLE_OBJECT",
        TEST_CLASS["ambiguous(java.lang.Double)"](1) +'');

    testcases[testcases.length] = new TestCase(
        "TEST_CLASS[\"ambiguous(double)\"](1)",
        "DOUBLE",
        TEST_CLASS["ambiguous(double)"](1) +'');

    test();