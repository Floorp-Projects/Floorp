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
 *  File Name:          eval-001.js
 *  Description:
 *
 *  For JavaScript 1.4:
 *
 *  The global eval function may not be accessed indirectly and then called.
 *  This feature will continue to work in JavaScript 1.3 but will result in an
 *  error in JavaScript 1.4. This restriction is also in place for the With and
 *  Closure constructors.
 *
 *  In this test, the indirect call to eval should succeed.
 *
 *  Author:             christine@netscape.com
 *  Date:               11 August 1998
 */
    var SECTION = "eval-001.js";
    var VERSION = "JS1_3";
    var TITLE   = "Calling eval indirectly should fail in version 140";
    var BUGNUMBER =

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = new Array();

    var MY_EVAL = eval;
    var RESULT = "Failed:  indirect call to eval failed.";
    var EXPECT = "Passed:  indirect call to eval succeeded.";

    MY_EVAL( "RESULT = EXPECT" );

    testcases[tc++] = new TestCase(
        SECTION,
        "Call eval indirectly",
        EXPECT,
        RESULT );

    test();
