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
        File Name:      method-006-n.js
        Description:

        Assigning a Java method to a JavaScript object should not change the
        context associated with the Java method -- its this object should
        be the Java object, not the JavaScript object.

        That is from Flanagan.  In practice, this fails all implementations.

        @author     christine@netscape.com
        @version    1.00
*/
    var SECTION = "LiveConnect Objects";
    var VERSION = "1_3";
    var TITLE   = "Assigning a Non-Static Java Method to a JavaScript Object";

    var testcases = new Array();

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);

    var java_string = new java.lang.String("LiveConnect");
    var js_string   = "JavaScript";

    js_string.startsWith = java_string.startsWith;

    testcases[testcases.length] = new TestCase(
        SECTION,
        "var java_string = new java.lang.String(\"LiveConnect\");" +
        "var js_string = \"JavaScript\"" +
        "js_string.startsWith = java_string.startsWith"+
        "js_string.startsWith(\"J\")",
        false,
        js_string.startsWith("J") );

    test();

function MyObject() {
    this.println = java.lang.System.out.println;
    this.classForName = java.lang.Class.forName;
    return this;
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
