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
    File Name:          println-001.js
    Section:       LiveConnect
    Description:

    Regression test for
    http://scopus.mcom.com/bugsplat/show_bug.cgi?id=114820

    Verify that java.lang.System.out.println does not cause an error.
    Not sure how to get to the error any other way.

    Author:             christine@netscape.com
    Date:               12 november 1997
*/
    var SECTION = "println-001.js";
    var VERSION = "JS1_3";
    var TITLE   = "java.lang.System.out.println";

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = new Array();

    testcases[testcases.length] = new TestCase(
        SECTION,
        "java.lang.System.out.println( \"output from test live/Methods/println-001.js\")",
        void 0,
        java.lang.System.out.println( "output from test live/Methods/println-001.js" ) );

    test();

function test() {
    for ( tc=0; tc < testcases.length; tc++ ) {
        testcases[tc].passed = writeTestCaseResult(
                            testcases[tc].expect,
                            testcases[tc].actual,
                            testcases[tc].description +" = "+
                            testcases[tc].actual );

        testcases[tc].reason += ( testcases[tc].passed ) ? "" : "wrong value ";
    }
    stopTest();
    return ( testcases );
}
