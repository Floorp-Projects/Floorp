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
 *  Pass a JavaScript number to ambiguous method.
 *
 */
    var SECTION = "Preferred argument conversion:  undefined";
    var VERSION = "1_4";
    var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
                    SECTION;
    startTest();

    var TEST_CLASS = new
        Packages.com.netscape.javascript.qa.lc3.number.Number_008;

    testcases[testcases.length] = new TestCase(
        "TEST_CLASS.ambiguous(1)",
        TEST_CLASS.expect(),
        TEST_CLASS.ambiguous(1) );

    test();