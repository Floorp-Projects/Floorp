/* The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * 
 */
/**
 * Preferred Argument Conversion.
 *
 * Use the syntax for explicit method invokation to override the default
 * preferred argument conversion.
 *
 */
    var SECTION = "Preferred argument conversion:  boolean";
    var VERSION = "1_4";
    var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
                    SECTION;
    startTest();

    var TEST_CLASS = new
        Packages.com.netscape.javascript.qa.lc3.bool.Boolean_001;

    // invoke method that accepts java.lang.Boolean

    testcases[testcases.length] = new TestCase(
        "TEST_CLASS[\"ambiguous(java.lang.Boolean)\"](true)",
        TEST_CLASS.BOOLEAN_OBJECT,
        TEST_CLASS["ambiguous(java.lang.Boolean)"](true) );

    testcases[testcases.length] = new TestCase(
        "TEST_CLASS[\"ambiguous(java.lang.Boolean)\"](false)",
        TEST_CLASS.BOOLEAN_OBJECT,
        TEST_CLASS["ambiguous(java.lang.Boolean)"](false) );

    // invoke method that expects java.lang.Object

    testcases[testcases.length] = new TestCase(
        "TEST_CLASS[\"ambiguous(java.lang.Object)\"](true)",
        TEST_CLASS.OBJECT,
        TEST_CLASS["ambiguous(java.lang.Object)"](true) );

    testcases[testcases.length] = new TestCase(
        "TEST_CLASS[\"ambiguous(java.lang.Boolean)\"](false)",
        TEST_CLASS.OBJECT,
        TEST_CLASS["ambiguous(java.lang.Object)"](false) );

    // invoke method that expects a String

    testcases[testcases.length] = new TestCase(
        "TEST_CLASS[\"ambiguous(java.lang.String)\"](true)",
        TEST_CLASS.STRING,
        TEST_CLASS["ambiguous(java.lang.String)"](true) );

    testcases[testcases.length] = new TestCase(
        "TEST_CLASS[\"ambiguous(java.lang.Boolean)\"](false)",
        TEST_CLASS.STRING,
        TEST_CLASS["ambiguous(java.lang.String)"](false) );

    test();