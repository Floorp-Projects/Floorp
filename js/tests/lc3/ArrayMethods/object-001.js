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
 *  java array objects "inherit" JS string methods.  verify that byte arrays
 *  can inherit JavaScript Array object methods join, reverse, sort and valueOf
 *
 */
    var SECTION = "java array object inheritance JavaScript Array methods";
    var VERSION = "1_4";
    var TITLE   = "LiveConnect 3.0 " + SECTION;

    startTest();

    dt = new Packages.com.netscape.javascript.qa.liveconnect.DataTypeClass();

    obArray = dt.PUB_ARRAY_OBJECT;

    // check string value

    testcases[tc++] = new TestCase(
        "dt = new Packages.com.netscape.javascript.qa.liveconnect.DataTypeClass(); "+
        "obArray = dt.PUB_ARRAY_OBJECT" +
        "obArray.join() +''",
        join(obArray),
        obArray.join() );

    // check type of object returned by method

    testcases[tc++] = new TestCase(
        "typeof obArray.reverse().join()",
        reverse(obArray),
        obArray.reverse().join() );

    testcases[tc++] = new TestCase(
        "obArray.reverse().getClass().getName() +''",
        "[Ljava.lang.Object;",
        obArray.reverse().getClass().getName() +'');

    test();

    function join( a ) {
        for ( var i = 0, s = ""; i < a.length; i++ ) {
            s += a[i].toString() + ( i + 1 < a.length ? "," : "" );
        }
        return s;
    }
    function reverse( a ) {
        for ( var i = a.length -1, s = ""; i >= 0; i-- ) {
            s += a[i].toString() + ( i> 0 ? "," : "" );
        }
        return s;
    }