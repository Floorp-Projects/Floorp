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
        File Name:      object-004.js
        Description:

        Getting and Setting Java Object properties by index value.

        @author     christine@netscape.com
        @version    1.00
*/
    var SECTION = "LiveConnect Objects";
    var VERSION = "1_3";
    var TITLE   = "Getting and setting JavaObject properties by index value";

    var testcases = new Array();

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);

    var vector = new java.util.Vector();

    testcases[testcases.length] = new TestCase(
        SECTION,
        "var vector = new java.util.Vector(); vector.addElement(\"hi\")",
        void 0,
        vector.addElement("hi") );

    testcases[testcases.length] = new TestCase(
        SECTION,
        "vector.elementAt(0) +''",
        "hi",
        vector.elementAt(0)+"" );

    testcases[testcases.length] = new TestCase(
        SECTION,
        "vector.setElementAt( \"hello\", 0)",
        void 0,
        vector.setElementAt( "hello", 0) );

    testcases[testcases.length] = new TestCase(
        SECTION,
        "vector.elementAt(0) +''",
        "hello",
        vector.elementAt(0)+"" );

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
