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
 *  The Java language allows static methods to be invoked using either the
 *  class name or a reference to an instance of the class, but previous
 *  versions of liveocnnect only allowed the former.
 *
 *  Verify that we can call static methods and get the value of static fields
 *  from an instance reference.
 *
 *  author: christine@netscape.com
 *
 *  date:  12/9/1998
 *
 */
    var SECTION = "Call static methods from an instance";
    var VERSION = "1_4";
    var TITLE   = "LiveConnect 3.0 " +
                    SECTION;
    startTest();

    var DT = Packages.com.netscape.javascript.qa.liveconnect.DataTypeClass;
    var dt = new DT();

    var a = new Array;
    var i = 0;

    a[i++] = new TestObject(
        "dt.staticSetDoubleObject( 99 )",
        "dt.PUB_STATIC_DOUBLE_OBJECT.doubleValue()",
        "dt.staticGetDoubleObject().doubleValue()",
        "dt.staticGetDoubleObject().getClass().getName() +''",
        99,
        "java.lang.Double" );

    a[i++] = new TestObject(
        "dt.staticSetShortObject( new java.lang.Short(32109) )",
        "dt.PUB_STATIC_SHORT_OBJECT.doubleValue()",
        "dt.staticGetShortObject().doubleValue()",
        "dt.staticGetShortObject().getClass().getName() +''",
        32109,
        "java.lang.Short" );

    a[i++] = new TestObject(
        "dt.staticSetIntegerObject( new java.lang.Integer(2109876543) )",
        "dt.PUB_STATIC_INTEGER_OBJECT.doubleValue()",
        "dt.staticGetIntegerObject().doubleValue()",
        "dt.staticGetIntegerObject().getClass().getName() +''",
        2109876543,
        "java.lang.Integer" );


    a[i++] = new TestObject(
        "dt.staticSetLongObject( new java.lang.Long(9012345678901234567) )",
        "dt.PUB_STATIC_LONG_OBJECT.doubleValue()",
        "dt.staticGetLongObject().doubleValue()",
        "dt.staticGetLongObject().getClass().getName() +''",
        9012345678901234567,
        "java.lang.Long" );


    a[i++] = new TestObject(
        "dt.staticSetDoubleObject(new java.lang.Double( java.lang.Double.MIN_VALUE) )",
        "dt.PUB_STATIC_DOUBLE_OBJECT.doubleValue()",
        "dt.staticGetDoubleObject().doubleValue()",
        "dt.staticGetDoubleObject().getClass().getName()+''",
        java.lang.Double.MIN_VALUE,
        "java.lang.Double" );

    a[i++] = new TestObject(
        "dt.staticSetFloatObject( new java.lang.Float(java.lang.Float.MIN_VALUE) )",
        "dt.PUB_STATIC_FLOAT_OBJECT.doubleValue()",
        "dt.staticGetFloatObject().doubleValue()",
        "dt.staticGetFloatObject().getClass().getName() +''",
        java.lang.Float.MIN_VALUE,
        "java.lang.Float" );

    a[i++] = new TestObject(
        "dt.staticSetCharacter( new java.lang.Character(45678) )",
        "dt.PUB_STATIC_CHAR_OBJECT.charValue()",
        "dt.staticGetCharacter().charValue()",
        "dt.staticGetCharacter().getClass().getName()+''",
        45678,
        "java.lang.Character" );

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
