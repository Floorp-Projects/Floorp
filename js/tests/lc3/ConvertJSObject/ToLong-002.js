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
 *  Preferred Argument Conversion.
 *
 *  Passing a JavaScript boolean to a Java method should prefer to call
 *  a Java method of the same name that expects a Java boolean.
 *
 */
    var SECTION = "Preferred argument conversion:  JavaScript Object to Long";
    var VERSION = "1_4";
    var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
                    SECTION;
    startTest();

    var TEST_CLASS = new Packages.com.netscape.javascript.qa.lc3.jsobject.JSObject_006;

    function MyFunction() {
        return "hello";
    }
    MyFunction.valueOf = new Function( "return 999" );

    function MyOtherFunction() {
        return "goodbye";
    }
    MyOtherFunction.valueOf = null;
    MyOtherFunction.toString = new Function( "return 999" );

    function MyObject(value) {
        this.value = value;
        this.valueOf = new Function("return this.value");
    }

    function MyOtherObject(stringValue) {
        this.stringValue = String( stringValue );
        this.toString = new Function( "return this.stringValue" );
        this.valueOf = null;
    }

    function AnotherObject( value ) {
        this.value = value;
        this.valueOf = new Function( "return this.value" );
        this.toString = new Function( "return 666" );
    }

    // should pass MyFunction.valueOf() to ambiguous

    testcases[testcases.length] = new TestCase(
        "TEST_CLASS.ambiguous( MyFunction ) +''",
        "LONG",
        TEST_CLASS.ambiguous( MyFunction )+'' );

    // should pass MyOtherFunction.toString() to ambiguous

    testcases[testcases.length] = new TestCase(
        "TEST_CLASS.ambiguous( MyOtherFunction ) +''",
        "LONG",
        TEST_CLASS.ambiguous( MyOtherFunction )+'' );

    // should pass MyObject.valueOf() to ambiguous

    testcases[testcases.length] = new TestCase(
        "TEST_CLASS.ambiguous( new MyObject(12345) ) +''",
        "LONG",
        TEST_CLASS.ambiguous( new MyObject(12345) )+'' );

    // should pass MyOtherObject.toString() to ambiguous

    testcases[testcases.length] = new TestCase(
        "TEST_CLASS.ambiguous( new MyOtherObject(\"12345\") ) +''",
        "LONG",
        TEST_CLASS.ambiguous( new MyOtherObject("12345") )+'' );

    // should pass AnotherObject.valueOf() to ambiguous

        testcases[testcases.length] = new TestCase(
        "TEST_CLASS.ambiguous( new AnotherObject(\"12345\") ) +''",
        "LONG",
        TEST_CLASS.ambiguous( new AnotherObject("12345") )+'' );

    test();

