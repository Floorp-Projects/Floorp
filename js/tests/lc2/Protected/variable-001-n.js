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
        File Name:      variable-001-n.js
        Description:

        Attempt to access a protected variable should fail.

        In LiveConnect v 1, it failed silently, and accessing a protected
        member returned undefined.

        In LiveConnect v 2, it fails with the same error that occurs when
        referring to a method or field that does not exist.

        @author     christine@netscape.com
        @version    1.00
*/
    var SECTION = "LiveConnect Objects";
    var VERSION = "1_3";
    var TITLE   = "Accessing protected variables";

    var testcases = new Array();

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);

    var vector = new java.util.Vector();
    vector.addElement("hi");

    testcases[testcases.length] = new TestCase(
        SECTION,
        "var vector = new java.util.Vector(); vector.addElement(\"hi\"); String(vector.elementAt(0))",
        "hi",
        String(vector.elementAt(0)) );

    testcases[testcases.length] = new TestCase(
        SECTION,
        "Number(vector.size())",
        1,
        Number(vector.size()) );

    testcases[testcases.length] = new TestCase(
        SECTION,
        "vector.elementCount",
        void 0,
        vector.elementCount );

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
