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
        Template for LiveConnect Tests

        File Name:      number-001.js
        Description:

        This test fails in lc3, but will succeed if the underlying version
        of liveconnect only supports LC2.

        @author     christine@netscape.com
        @version    1.00
*/
    var SECTION = "LiveConnect";
    var VERSION = "1_3";
    var TITLE   = "LiveConnect JavaScript to Java Data Type Conversion";

    var testcases = new Array();

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);

    // typeof all resulting objects is "object";
    var E_TYPE = "object";

    // JS class of all resulting objects is "JavaObject";
    var E_JSCLASS = "[object JavaObject]";

    var a = new Array();
    var i = 0;

    a[i++] = new TestObject( "java.lang.Long.toString(NaN)",
        java.lang.Long.toString(NaN), "0" );

    for ( var i = 0; i < a.length; i++ ) {

        // check typeof
        testcases[testcases.length] = new TestCase(
            SECTION,
            "typeof (" + a[i].description +")",
            a[i].type,
            typeof a[i].javavalue );
/*
        // check the js class
        testcases[testcases.length] = new TestCase(
            SECTION,
            "("+ a[i].description +").getJSClass()",
            E_JSCLASS,
            a[i].jsclass );
*/
        // check the number value of the object
        testcases[testcases.length] = new TestCase(
            SECTION,
            "String(" + a[i].description +")",
            a[i].jsvalue,
            String( a[i].javavalue ) );
    }

    test();

function TestObject( description, javavalue, jsvalue ) {
    this.description = description;
    this.javavalue = javavalue;
    this.jsvalue = jsvalue;
    this.type = E_TYPE;
//  LC2 does not support the __proto__ property in Java objects.
//    this.javavalue.__proto__.getJSClass = Object.prototype.toString;
//    this.jsclass = this.javavalue.getJSClass();
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
