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
        File Name:      char-002.js
        Description:

        Java fields whose value is char should be read as a JavaScript number.

        To test this:

        1.  Instantiate a Java object that has fields with char values,
            or reference a classes static field whose value is a char.
        2.  Reference the field, and set the value of a JavaScript variable
            to that field's value.
        3.  Check the value of the returned type, which should be "number"
        4.  Check the type of the returned type, which should be a number

        It is an error if the JavaScript variable is an object, or JavaObject
        whose class is java.lang.Character.

        @author     christine@netscape.com
        @version    1.00
*/
    var SECTION = "LiveConnect";
    var VERSION = "1_3";
    var TITLE   = "Java char return value to JavaScript Object";

    var testcases = new Array();

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);

//  In all cases, the expected type is "number"
    var E_TYPE = "number";

    //  Create arrays of actual results (java_array) and expected results
    //  (test_array).

    var java_array = new Array();
    var test_array = new Array();

    var i = 0;

    // Get File.separator char

    var os = java.lang.System.getProperty( "os.name" );
    var v;

    if ( os.startsWith( "Windows" ) ) {
        v = 92;
    } else {
        if ( os.startsWith( "Mac" ) ) {
            v = 58;
        } else {
            v = 47;
        }
    }

    java_array[i] = new JavaValue(  java.io.File.separatorChar   );
    test_array[i] = new TestValue(  "java.io.File.separatorChar", v );

    i++;

    for ( i = 0; i < java_array.length; i++ ) {
        CompareValues( java_array[i], test_array[i] );

    }

    test();

function CompareValues( javaval, testval ) {
    //  Check value
    testcases[testcases.length] = new TestCase( SECTION,
                                                testval.description,
                                                testval.value,
                                                javaval.value );
    //  Check type, which should be E_TYPE
    testcases[testcases.length] = new TestCase( SECTION,
                                                "typeof (" + testval.description +")",
                                                testval.type,
                                                javaval.type );

}
function JavaValue( value ) {
    this.value  = value;
    this.type   = typeof value;
    return this;
}
function TestValue( description, value ) {
    this.description = description;
    this.value = value;
    this.type =  E_TYPE;
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
