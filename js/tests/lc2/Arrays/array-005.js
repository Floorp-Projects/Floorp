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
        File Name:      array-005.js
        Description:

        Put and Get JavaArray Elements

        @author     christine@netscape.com
        @version    1.00
*/
    var SECTION = "LiveConnect";
    var VERSION = "1_3";
    var TITLE   = "Java Array to JavaScript JavaArray object";

    var testcases = new Array();

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);

    //  In all test cases, the expected type is "object, and the expected
    //  class is "JavaArray"

    var E_TYPE = "object";
    var E_CLASS = "[object JavaArray]";

    var byte_array = ( new java.lang.String("hi") ).getBytes();

    testcases[testcases.length] = new TestCase(
        SECTION,
        "byte_array = new java.lang.String(\"hi\")).getBytes(); delete byte_array.length",
        false,
        delete byte_array.length );

    testcases[testcases.length] = new TestCase(
        SECTION,
        "byte_array[0]",
        ("hi").charCodeAt(0),
        byte_array[0]);

    testcases[testcases.length] = new TestCase(
        SECTION,
        "byte_array[1]",
        ("hi").charCodeAt(1),
        byte_array[1]);

    byte_array.length = 0;

    testcases[testcases.length] = new TestCase(
        SECTION,
        "byte_array.length = 0; byte_array.length",
        2,
        byte_array.length );

    var properties = "";
    for ( var p in byte_array ) {
        properties += ( p == "length" ) ? p : "";
    }


    testcases[testcases.length] = new TestCase(
        SECTION,
        "for ( var p in byte_array ) { properties += p ==\"length\" ? p : \"\" }; properties",
        "",
        properties );

    testcases[testcases.length] = new TestCase(
        SECTION,
        "byte_array[\"length\"]",
        2,
        byte_array["length"] );

    byte_array["0"] = 127;

    testcases[testcases.length] = new TestCase(
        SECTION,
        "byte_array[\"0\"] = 127; byte_array[0]",
        127,
        byte_array[0] );

    byte_array[1] = 99;

    testcases[testcases.length] = new TestCase(
        SECTION,
        "byte_array[1] = 99; byte_array[\"1\"]",
        99,
        byte_array["1"] );

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
