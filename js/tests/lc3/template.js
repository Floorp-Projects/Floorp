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
 *  Template for LiveConnect 3.0 tests.
 *
 *
 */
    var SECTION = "undefined conversion";
    var VERSION = "1_4";
    var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
                    SECTION;
    startTest();

    // typeof all resulting objects is "object";
    var E_TYPE = "object";

    // JS class of all resulting objects is "JavaObject";
    var E_JSCLASS = "[object JavaObject]";

    var a = new Array();
    var i = 0;

    a[i++] = new TestObject( "java.lang.Long.toString(0)",
        java.lang.Long.toString(0), "0" );

    a[i++] = new TestObject( "java.lang.Long.toString(NaN)",
        java.lang.Long.toString(NaN), "0" );

    a[i++] = new TestObject( "java.lang.Long.toString(5)",
        java.lang.Long.toString(5), "5" );

    a[i++] = new TestObject( "java.lang.Long.toString(9.9)",
        java.lang.Long.toString(9.9), "9" );

    a[i++] = new TestObject( "java.lang.Long.toString(-9.9)",
        java.lang.Long.toString(-9.9), "-9" );

    for ( var i = 0; i < a.length; i++ ) {

        // check typeof
        testcases[testcases.length] = new TestCase(
            SECTION,
            "typeof (" + a[i].description +")",
            a[i].type,
            typeof a[i].javavalue );

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
    return this;
}
