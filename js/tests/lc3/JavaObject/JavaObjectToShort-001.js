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
 *  This test passes JavaScript number values to several Java methods
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
    var SECTION = "JavaObject to short";
    var VERSION = "1_4";
    var TITLE   = "LiveConnect 3.0 JavaScript to Java Data Type Conversion " +
                    SECTION;
    var BUGNUMBER = "335589";

    startTest();

    var dt = new DT();

    var a = new Array();
    var i = 0;

    // 1. If the object does not have a doubleValue() method, a runtime error
    //    occurs.
    // 2. Call the object's doubleValue() method
    // 3. If result(2) is NaN, a runtime error occurs
    // 3. Convert Result(2) to Java numeric type. Round JS number to integral
    //    value using round-to-negative-infinity mode.
    //    Numbers with a magnitude too large to be represented in the target
    //    integral type should result in a runtime error.

    a[i++] = new TestObject (
         "dt.PUB_DOUBLE_REPRESENTATION = 0.5 ;"+
         "dt.setShort( dt )",
         "dt.PUB_SHORT",
         "dt.getShort()",
         "typeof dt.getShort()",
         0,
         "number" );

    a[i++] = new TestObject (
         "dt.PUB_DOUBLE_REPRESENTATION = -0.5;"+
         "dt.setShort( dt )",
         "dt.PUB_SHORT",
         "dt.getShort()",
         "typeof dt.getShort()",
         0,
         "number" );

    a[i++] = new TestObject (
         "dt.PUB_DOUBLE_REPRESENTATION = 0.5;"+
         "dt.setShort( dt )",
         "dt.PUB_SHORT",
         "dt.getShort()",
         "typeof dt.getShort()",
         0,
         "number" );

    a[i++] = new TestObject (
         "dt.PUB_DOUBLE_REPRESENTATION = -0.4;"+
         "dt.setShort( dt )",
         "dt.PUB_SHORT",
         "dt.getShort()",
         "typeof dt.getShort()",
         0,
         "number" );

    a[i++] = new TestObject (
         "dt.PUB_DOUBLE_REPRESENTATION = 0.6;"+
         "dt.setShort( dt )",
         "dt.PUB_SHORT",
         "dt.getShort()",
         "typeof dt.getShort()",
         0,
         "number" );

    a[i++] = new TestObject (
         "dt.PUB_DOUBLE_REPRESENTATION = -0.6;"+
         "dt.setShort( dt )",
         "dt.PUB_SHORT",
         "dt.getShort()",
         "typeof dt.getShort()",
         0,
         "number" );

    a[i++] = new TestObject (
         "dt.PUB_DOUBLE_REPRESENTATION = "+Math.PI+";"+
         "dt.setShort( dt )",
         "dt.PUB_SHORT",
         "dt.getShort()",
         "typeof dt.getShort()",
         3,
         "number" );

    a[i++] = new TestObject (
         "dt.PUB_DOUBLE_REPRESENTATION = -" +Math.PI+";"+
         "dt.setShort( dt )",
         "dt.PUB_SHORT",
         "dt.getShort()",
         "typeof dt.getShort()",
         -3,
         "number" );

    a[i++] = new TestObject (
         "dt.PUB_DOUBLE_REPRESENTATION = 32767;"+
         "dt.setShort( dt )",
         "dt.PUB_SHORT",
         "dt.getShort()",
         "typeof dt.getShort()",
         32767,
         "number" );

    a[i++] = new TestObject (
         "dt.PUB_DOUBLE_REPRESENTATION = -32768;"+
         "dt.setShort( dt )",
         "dt.PUB_SHORT",
         "dt.getShort()",
         "typeof dt.getShort()",
         -32768,
         "number" );

    a[i++] = new TestObject (
         "dt.PUB_DOUBLE_REPRESENTATION = -32768.5;"+
         "dt.setShort( dt )",
         "dt.PUB_SHORT",
         "dt.getShort()",
         "typeof dt.getShort()",
         -32768,
         "number" );

    a[i++] = new TestObject (
         "dt.PUB_DOUBLE_REPRESENTATION = 32767.5;"+
         "dt.setShort( dt )",
         "dt.PUB_SHORT",
         "dt.getShort()",
         "typeof dt.getShort()",
         32767,
         "number" );

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
    this.javaTypeValue = typeof this.javaFieldValue;

    this.jsValue   = jsValue;
    this.jsType      = jsType;
}
