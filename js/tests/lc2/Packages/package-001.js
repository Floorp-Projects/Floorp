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
        File Name:      package-001.js
        Description:

        @author     christine@netscape.com
        @version    1.00
*/
    var SECTION = "LiveConnect Packages";
    var VERSION = "1_3";
    var TITLE   = "LiveConnect Packages";

    var testcases = new Array();

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);

    //  All packages are of the type "object"
    var E_TYPE = "object";

    //  The JavaScript [[Class]] property for all Packages is "[JavaPackage <packagename>]"
    var E_JSCLASS = "[JavaPackage ";

    //  Create arrays of actual results (java_array) and
    //  expected results (test_array).

    var java_array = new Array();
    var test_array = new Array();

    var i = 0;

    java_array[i] = new JavaValue(  java  );
    test_array[i] = new TestValue(  "java" );
    i++;

    java_array[i] = new JavaValue(  java.awt  );
    test_array[i] = new TestValue(  "java.awt" );
    i++;

    java_array[i] = new JavaValue(  java.beans  );
    test_array[i] = new TestValue(  "java.beans" );
    i++;

    java_array[i] = new JavaValue(  java.io  );
    test_array[i] = new TestValue(  "java.io" );
    i++;

    java_array[i] = new JavaValue(  java.lang  );
    test_array[i] = new TestValue(  "java.lang" );
    i++;

    java_array[i] = new JavaValue(  java.math  );
    test_array[i] = new TestValue(  "java.math" );
    i++;

    java_array[i] = new JavaValue(  java.net  );
    test_array[i] = new TestValue(  "java.net" );
    i++;

    java_array[i] = new JavaValue(  java.rmi  );
    test_array[i] = new TestValue(  "java.rmi" );
    i++;

    java_array[i] = new JavaValue(  java.text  );
    test_array[i] = new TestValue(  "java.text" );
    i++;

    java_array[i] = new JavaValue(  java.util  );
    test_array[i] = new TestValue(  "java.util" );
    i++;

    java_array[i] = new JavaValue(  Packages.javax  );
    test_array[i] = new TestValue(  "Packages.javax" );
    i++;

    java_array[i] = new JavaValue(  Packages.javax.javascript  );
    test_array[i] = new TestValue(  "Packages.javax.javascript" );
    i++;

    java_array[i] = new JavaValue(  Packages.javax.javascript.examples  );
    test_array[i] = new TestValue(  "Packages.javax.javascript.examples" );
    i++;

    for ( i = 0; i < java_array.length; i++ ) {
        CompareValues( java_array[i], test_array[i] );

    }

    test();
function CompareValues( javaval, testval ) {
    //  Check typeof, which should be E_TYPE
    testcases[testcases.length] = new TestCase( SECTION,
                                                "typeof (" + testval.description +")",
                                                testval.type,
                                                javaval.type );

    //  Check JavaScript class, which should be E_JSCLASS + the package name
    testcases[testcases.length] = new TestCase( SECTION,
                                                "(" + testval.description +").getJSClass()",
                                                testval.jsclass,
                                                javaval.jsclass );
}
function JavaValue( value ) {
    this.value  = value;
    this.type   = typeof value;
    this.jsclass = value +""
    return this;
}
function TestValue( description ) {
    this.packagename = (description.substring(0, "Packages.".length) ==
        "Packages.") ? description.substring("Packages.".length, description.length ) :
        description;

    this.description = description;
    this.type =  E_TYPE;
    this.jsclass = E_JSCLASS +  this.packagename +"]";
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
