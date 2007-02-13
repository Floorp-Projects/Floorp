/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
    var SECTION = "e11_6_1_3";
    var VERSION = "ECMA_1";
    startTest();
    var testcases = getTestCases();

    writeHeaderToLog( SECTION + " The Addition operator ( + )");
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    // tests for boolean primitive, boolean object, Object object, a "MyObject" whose value is
    // a boolean primitive and a boolean object, 

    var DATE1 = new Date(0);

    array[item++] = new TestCase(   SECTION,
                                    "var DATE1 = new Date(); DATE1 + DATE1",
                                    DATE1.toString() + DATE1.toString(),
                                    DATE1 + DATE1 );

    array[item++] = new TestCase(   SECTION,
                                    "var DATE1 = new Date(); DATE1 + 0",
                                    DATE1.toString() + 0,
                                    DATE1 + 0 );

    array[item++] = new TestCase(   SECTION,
                                    "var DATE1 = new Date(); DATE1 + new Number(0)",
                                    DATE1.toString() + 0,
                                    DATE1 + new Number(0) );

    array[item++] = new TestCase(   SECTION,
                                    "var DATE1 = new Date(); DATE1 + true",
                                    DATE1.toString() + "true",
                                    DATE1 + true );

    array[item++] = new TestCase(   SECTION,
                                    "var DATE1 = new Date(); DATE1 + new Boolean(true)",
                                    DATE1.toString() + "true",
                                    DATE1 + new Boolean(true) );

    array[item++] = new TestCase(   SECTION,
                                    "var DATE1 = new Date(); DATE1 + new Boolean(true)",
                                    DATE1.toString() + "true",
                                    DATE1 + new Boolean(true) );

    var MYOB1 = new MyObject( DATE1 );
    var MYOB2 = new MyValuelessObject( DATE1 );
    //var MYOB3 = new MyProtolessObject( DATE1 );
    //var MYOB4 = new MyProtoValuelessObject( DATE1 );

    array[item++] = new TestCase(   SECTION,
                                    "MYOB1 = new MyObject(DATE1); MYOB1 + new Number(1)",
                                    "[object Object]1",
                                    MYOB1 + new Number(1) );

    array[item++] = new TestCase(   SECTION,
                                    "MYOB1 = new MyObject(DATE1); MYOB1 + 1",
                                    "[object Object]1",
                                    MYOB1 + 1 );

/*    array[item++] = new TestCase(   SECTION,
                                    "MYOB2 = new MyValuelessObject(DATE1); MYOB3 + 'string'",
                                    DATE1.toString() + "string",
                                    MYOB2 + 'string' );

    array[item++] = new TestCase(   SECTION,
                                    "MYOB2 = new MyValuelessObject(DATE1); MYOB3 + new String('string')",
                                    DATE1.toString() + "string",
                                    MYOB2 + new String('string') );

    array[item++] = new TestCase(   SECTION,
                                    "MYOB3 = new MyProtolessObject(DATE1); MYOB3 + new Boolean(true)",
                                    DATE1.toString() + "true",
                                    MYOB3 + new Boolean(true) );
*/
    array[item++] = new TestCase(   SECTION,
                                    "MYOB1 = new MyObject(DATE1); MYOB1 + true",
                                    "[object Object]true",
                                    MYOB1 + true );

    return ( array );
}

/*  cn:  __proto__ is not ecma3 compliant

function MyProtoValuelessObject() {
//this.valueOf = new Function ( "" );
    this.valueOf = function (){ ""};
    this.__proto__ = null;
}
function MyProtolessObject( value ) {
//    this.valueOf = new Function( "return this.value" );
    this.valueOf = function(){return this.value};
    this.__proto__ = null;
    this.value = value;
}
*/
function MyValuelessObject(value) {
    //this.__proto__ = new MyPrototypeObject(value);
    this.constructor.prototype = new MyPrototypeObject(value);
}
function MyPrototypeObject(value) {
    this.valueOf = function(){return this.value};
    this.toString = function(){return this.value + ''};
    this.value = value;
}
function MyObject( value ) {
    this.valueOf = function(){return this.value};
    this.value = value;
}
