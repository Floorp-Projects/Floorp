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
/* -*- Mode: java; tab-width: 8 -*-
 * Copyright (C) 1997, 1998 Netscape Communications Corporation,
 * All Rights Reserved.
 */

/**
 *  JavaScript to Java type conversion.
 *
 *  This test passes JavaScript boolean values to several Java methods
 *  that expect arguments of various types, and verifies that the value is
 *  converted to the correct value and type.
 *
 *  This tests instance methods, and not static methods.
 *
 *  Running these tests successfully requires you to have
 *  com.netscape.javascript.qa.liveconnect.DataTypeClass on your classpath.
 *
 *  Specification:  Method Overloading Proposal for Liveconnect 3.0
 *
 *  @author: christine@netscape.com
 *
 */
    var SECTION = "boolean conversion";
    var VERSION = "1_4";
    var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
                    SECTION;
    var BUGNUMBER = "335907";

    startTest();

    var dt = new DT();

    var a = new Array();
    var i = 0;

    // passing a boolean to a java method that expects a boolean should map
    // directly to the Java true / false equivalent

    a[i++] = new TestObject(
        "dt.setBoolean( true )",
        "dt.PUB_BOOLEAN",
        "dt.getBoolean()",
        "typeof dt.getBoolean()",
        true,
        "boolean" );

    a[i++] = new TestObject(
        "dt.setBoolean( false )",
        "dt.PUB_BOOLEAN",
        "dt.getBoolean()",
        "typeof dt.getBoolean()",
        false,
        "boolean" );

    // passing a boolean to a java method that expects a Boolean object
    // should convert to a new instance of java.lang.Boolean

    a[i++] = new TestObject(
        "dt.setBooleanObject( true )",
        "dt.PUB_BOOLEAN_OBJECT +''",
        "dt.getBooleanObject() +''",
        "dt.getBooleanObject().getClass()",
        "true",
        java.lang.Class.forName( "java.lang.Boolean") );

    a[i++] = new TestObject(
        "dt.setBooleanObject( false )",
        "dt.PUB_BOOLEAN_OBJECT +''",
        "dt.getBooleanObject() +''",
        "dt.getBooleanObject().getClass()",
        "false",
        java.lang.Class.forName( "java.lang.Boolean") );


    // passing a boolean to a java method that expects a java.lang.Object
    // should convert to a new instance of java.lang.Boolean

    a[i++] = new TestObject(
        "dt.setObject( true )",
        "dt.PUB_OBJECT +''",
        "dt.getObject() +''",
        "dt.getObject().getClass()",
        "true",
        java.lang.Class.forName( "java.lang.Boolean") );

    a[i++] = new TestObject(
        "dt.setObject( false )",
        "dt.PUB_OBJECT +''",
        "dt.getObject() +''",
        "dt.getObject().getClass()",
        "false",
        java.lang.Class.forName( "java.lang.Boolean") );

    // passing a boolean to a java method that expects a java.lang.String
    // should convert true to a java.lang.String whose value is "true" and
    // false to a java.lang.String whose value is "false"

    a[i++] = new TestObject(
        "dt.setStringObject( true )",
        "dt.PUB_STRING +''",
        "dt.getStringObject() +''",
        "dt.getStringObject().getClass()",
        "true",
        java.lang.Class.forName( "java.lang.String") );

    a[i++] = new TestObject(
        "dt.setStringObject( false )",
        "dt.PUB_STRING +''",
        "dt.getStringObject() +''",
        "dt.getStringObject().getClass()",
        "false",
        java.lang.Class.forName( "java.lang.String") );

    for ( i = 0; i < a.length; i++ ) {
        testcases[testcases.length] = new TestCase(
            a[i].description +"; "+ a[i].javaFieldName,
            a[i].jsValue,
            a[i].javaFieldValue );

        testcases[testcases.length] = new TestCase(
            a[i].description +"; " + a[i].javaMethodName,
            a[i].jsValue,
            a[i].javaMethodValue );

        testcases[testcases.length] = new TestCase(
            a[i].javaTypeName,
            a[i].jsType,
            a[i].javaTypeValue );
    }

    test();

function TestObject( description, javaField, javaMethod, javaType,
    jsValue, jsType )
{
    eval (description );

    this.description = description;
    this.javaFieldName = javaField;
    this.javaFieldValue = eval( javaField );
    this.javaMethodName = javaMethod;
    this.javaMethodValue = eval( javaMethod );
    this.javaTypeName = javaType,
    this.javaTypeValue = eval( javaType );

    this.jsValue   = jsValue;
    this.jsType      = jsType;
}
