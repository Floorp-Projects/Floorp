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
        File Name:      object-006.js
        Description:

        Attempt to construct a java.lang.Character.  currently this fails because of
        http://scopus/bugsplat/show_bug.cgi?id=106464

        @author     christine@netscape.com
        @version    1.00
*/

    var SECTION = "LiveConnect Objects";
    var VERSION = "1_3";
    var TITLE   = "Construct a java.lang.Character";

    var testcases = new Array();

    var error = err;

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);

    testcases[testcases.length] = new TestCase (
        SECTION,
        "var string = new java.lang.String(\"hi\"); "+
        "var c = new java.lang.Character(string.charAt(0)); String(c.toString())",
        "h",
        "" )

    var string = new java.lang.String("hi");
    var c = new java.lang.Character( string.charAt(0) );

    testcases[0].actual = String(c.toString());

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
