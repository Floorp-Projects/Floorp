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
 *  The Java language allows static methods to be invoked using either the
 *  class name or a reference to an instance of the class, but previous
 *  versions of liveocnnect only allowed the former.
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
        "dt.staticSetBoolean( true )",
        "dt.PUB_STATIC_BOOLEAN",
        "dt.staticGetBoolean()",
        "typeof dt.staticGetBoolean()",
        true,
        "boolean" );

    a[i++] = new TestObject(
        "dt.staticSetBoolean( false )",
        "dt.PUB_STATIC_BOOLEAN",
        "dt.staticGetBoolean()",
        "typeof dt.staticGetBoolean()",
        false,
        "boolean" );

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
