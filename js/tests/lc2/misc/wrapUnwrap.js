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
    File Name:          wrapUnwrap.js
    Section:            LiveConnect
    Description:

    Tests wrapping and unwrapping objects.
     @author mikeang

*/
    var SECTION = "wrapUnwrap.js";
    var VERSION = "JS1_3";
    var TITLE   = "LiveConnect";

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = new Array();


    var hashtable = new java.util.Hashtable();
    var sameHashtable = hashtable;

jsEquals(hashtable,sameHashtable);
javaEquals(hashtable,sameHashtable);

function returnString(theString) {
  return theString;
}
var someString = new java.lang.String("foo");
var sameString = returnString(someString);
jsEquals(someString,sameString);
javaEquals(someString,sameString);

var assignToProperty = new Object();
assignToProperty.assignedString = someString;
jsEquals(someString,assignToProperty.assignedString);
javaEquals(someString,assignToProperty.assignedString);

function laConstructor(a,b,c) {
  this.one = a;
  this.two = b;
  this.three = c;
}
var stack1 = new java.util.Stack();
var stack2 = new java.util.Stack();
var num = 28;
var constructed = new laConstructor(stack1, stack2, num);
javaEquals(stack1, constructed.one);
javaEquals(stack2, constructed.two);
jsEquals(num, constructed.three);

    test();

function jsEquals(expectedResult, actualResult, message) {
    testcases[testcases.length] = new TestCase( SECTION,
        expectedResult +" == "+actualResult,
        expectedResult,
        actualResult );
}

function javaEquals(expectedResult, actualResult, message) {
    testcases[testcases.length] = new TestCase( SECTION,
        expectedResult +" == "+actualResult,
        expectedResult,
        actualResult );
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
