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
        File Name:      null-001.js
        Description:

        When accessing a Java field whose value is null, JavaScript should read
        the value as the JavaScript null object.

        To test this:

        1.  Call a java method that returns the Java null value
        2.  Check the value of the returned object, which should be null
        3.  Check the type of the returned object, which should be "object"

        @author     christine@netscape.com
        @version    1.00
*/
    var SECTION = "LiveConnect";
    var VERSION = "1_3";
    var TITLE   = "Java null to JavaScript Object";

    var testcases = new Array();

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);
//  display test information

    var choice = new java.awt.Choice();

    testcases[testcases.length] = new TestCase(
        SECTION,
        "var choice = new java.awt.Choice(); choice.getSelectedObjects()",
        null,
        choice.getSelectedObjects() );

    testcases[testcases.length] = new TestCase(
        SECTION,
        "typeof choice.getSelectedObjects()",
        "object",
        typeof choice.getSelectedObjects() );



    test();

function CheckType( et, at ) {
}
function CheckValue( ev, av ) {
}
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
