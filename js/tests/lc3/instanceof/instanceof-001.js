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
 *  Verify that we can use the instanceof operator on java objects.
 *
 *
 */
    var SECTION = "instanceof";
    var VERSION = "1_4";
    var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
                    SECTION;
    startTest();


    testcases[tc++] = new TestCase(
        "\"hi\" instance of java.lang.String",
        false,
        "hi" instanceof java.lang.String );

    testcases[tc++] = new TestCase(
        "new java.lang.String(\"hi\") instanceof java.lang.String",
        true,
        new java.lang.String("hi") instanceof java.lang.String );

    testcases[tc++] = new TestCase(
        "new java.lang.String(\"hi\") instanceof java.lang.Object",
        true,
        new java.lang.String("hi") instanceof java.lang.Object );

    testcases[tc++] = new TestCase(
        "java.lang.String instanceof java.lang.Class",
        false,
        java.lang.String instanceof java.lang.Class );

    testcases[tc++] = new TestCase(
        "java.lang.Class.forName(\"java.lang.String\") instanceof java.lang.Class",
        true,
        java.lang.Class.forName("java.lang.String") instanceof java.lang.Class );

    testcases[tc++] = new TestCase(
        "new java.lang.Double(5.0) instanceof java.lang.Double",
        true,
        new java.lang.Double(5.0) instanceof java.lang.Double );

    testcases[tc++] = new TestCase(
        "new java.lang.Double(5.0) instanceof java.lang.Number",
        true,
        new java.lang.Double(5.0) instanceof java.lang.Number );

    testcases[tc++] = new TestCase(
        "new java.lang.String(\"hi\").getBytes() instanceof java.lang.Double",
        true,
        new java.lang.Double(5.0) instanceof java.lang.Double );




    test();