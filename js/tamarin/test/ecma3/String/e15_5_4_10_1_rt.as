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

/*
//s = new String('this is a string object');
//s.substring(Infinity, Infinity)
a = -22;
b = -a;
b = 0 - a;
x = -Infinity;
//y = Infinity;
*/

    var SECTION = "15.5.4.10-1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "String.prototype.substring( start, end )";
    
    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;
    array[item++] = new TestCase( SECTION,  "String.prototype.substring.length",        2,          String.prototype.substring.length );
    array[item++] = new TestCase( SECTION,  "delete String.prototype.substring.length", false,      delete String.prototype.substring.length );
    array[item++] = new TestCase( SECTION,  "delete String.prototype.substring.length; String.prototype.substring.length", 2,      (delete String.prototype.substring.length, String.prototype.substring.length) );

    // test cases for when substring is called with no arguments.

    // this is a string object

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); typeof s.substring()",
                                    "string",
                                    (s = new String('this is a string object'), typeof s.substring() ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String(''); s.substring(1,0)",
                                    "",
                                    (s = new String(''), s.substring(1,0) ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.substring(true, false)",
                                    "t",
                                    (s = new String('this is a string object'), s.substring(false, true) ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.substring(NaN, Infinity)",
                                    "this is a string object",
                                    (s = new String('this is a string object'), s.substring(NaN, Infinity) ) );


    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.substring(Infinity, NaN)",
                                    "this is a string object",
                                    (s = new String('this is a string object'), s.substring(Infinity, NaN) ) );


    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.substring(Infinity, Infinity)",
                                    "",
                                    (s = new String('this is a string object'), s.substring(Infinity, Infinity) ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.substring(-0.01, 0)",
                                    "",
                                    (s = new String('this is a string object'), s.substring(-0.01,0) ) );


    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.substring(s.length, s.length)",
                                    "",
                                    (s = new String('this is a string object'), s.substring(s.length, s.length) ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.substring(s.length+1, 0)",
                                    "this is a string object",
                                    (s = new String('this is a string object'), s.substring(s.length+1, 0) ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.substring(-Infinity, -Infinity)",
                                    "",
                                    (s = new String('this is a string object'), s.substring(-Infinity, -Infinity) ) );

    // this is not a String object, start is not an integer


    array[item++] = new TestCase(   SECTION,
                                    "var s = new Array(1,2,3,4,5); s.substring = String.prototype.substring; s.substring(Infinity,-Infinity)",
                                    "1,2,3,4,5",
                                    (s = new Array(1,2,3,4,5), s.substring = String.prototype.substring, s.substring(Infinity,-Infinity) ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new Array(1,2,3,4,5); s.substring = String.prototype.substring; s.substring(9,-Infinity)",
                                    "1,2,3,4,5",
                                    (s = new Array(1,2,3,4,5), s.substring = String.prototype.substring, s.substring(9,-Infinity) ) );


    array[item++] = new TestCase(   SECTION,
                                    "var s = new Array(1,2,3,4,5); s.substring = String.prototype.substring; s.substring(true, false)",
                                    "1",
                                    (s = new Array(1,2,3,4,5), s.substring = String.prototype.substring, s.substring(true, false) ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new Array(1,2,3,4,5); s.substring = String.prototype.substring; s.substring('4', '5')",
                                    "3",
                                    (s = new Array(1,2,3,4,5), s.substring = String.prototype.substring, s.substring('4', '5') ) );


    // this is an object object
    array[item++] = new TestCase(   SECTION,
                                    "var obj = new Object(); obj.substring = String.prototype.substring; obj.substring(8,0)",
                                    "[object ",
                                    (obj = new Object(), obj.substring = String.prototype.substring, obj.substring(8,0) ) );

    array[item++] = new TestCase(   SECTION,
                                    "var obj = new Object(); obj.substring = String.prototype.substring; obj.substring(8,obj.toString().length)",
                                    "Object]",
                                    (obj = new Object(), obj.substring = String.prototype.substring, obj.substring(8, obj.toString().length) ) );


    // this is a function object
   /* array[item++] = new TestCase(   SECTION,
                                    "var obj = function() {}; obj.substring = String.prototype.substring; obj.toString = Object.prototype.toString; obj.substring(8, Infinity)",
                                    "Function]",
                                    (obj = function() {}, obj.substring = String.prototype.substring,obj.toString = Object.prototype.toString; obj.substring(8,Infinity) ) );*/

    array[item++] = new TestCase(   SECTION,
                                    "var obj = function() {}; obj.substring = String.prototype.substring; obj.toString = Object.prototype.toString; obj.substring(8, Infinity)",
                                    " Function() {}",
                                    (obj = function() {}, obj.substring = String.prototype.substring, obj.substring(8,Infinity) ) );

    // this is a number object
    thisError="no error";
    try{
        var obj = new Number(NaN);
        obj.substring = String.prototype.substring;
        obj.substring(Infinity, NaN);
    }catch(e2:Error){
        thisError=e2.toString();
    }finally{
        array[item++] = new TestCase(   SECTION,
                                    "var obj = new Number(NaN); obj.substring = String.prototype.substring; obj.substring(Infinity, NaN)",
                                    "ReferenceError: Error #1056",
                                    thisError.substring(0,27) );
    }
    /*array[item++] = new TestCase(   SECTION,
                                    "var obj = new Number(NaN); obj.substring = String.prototype.substring; obj.substring(Infinity, NaN)",
                                    "NaN",
                                    (obj = new Number(NaN), obj.substring = String.prototype.substring, obj.substring(Infinity, NaN) ) );*/


    // this is the Math object
    array[item++] = new TestCase(   SECTION,
                                    "var obj = Math; obj.substring = String.prototype.substring; obj.substring(Math.PI, -10)",
                                    "[cl",
                                    (obj = Math, obj.substring = String.prototype.substring, obj.substring(Math.PI, -10) ) );

    // this is a Boolean object
    thisError="no error";
    try{
        var obj = new Boolean();
        obj.substring = String.prototype.substring;
        obj.substring(new Array(), new Boolean(1));
    }catch(e:Error){
        thisError=e.toString();
    }finally{
        array[item++] = new TestCase(   SECTION,
                                    "var obj = new Boolean(); obj.substring = String.prototype.substring; obj.substring(new Array(), new Boolean(1))",
                                    "ReferenceError: Error #1056",
                                    thisError.substring(0,27) );
    }


    
    /*array[item++] = new TestCase(   SECTION,
                                    "var obj = new Boolean(); obj.substring = String.prototype.substring; obj.substring(new Array(), new Boolean(1))",
                                    "f",
                                    (obj = new Boolean(), obj.substring = String.prototype.substring, obj.substring(new Array(), new Boolean(1)) ) );*/

    // this is a user defined object
    array[item++] = new TestCase( SECTION,
                                    "var obj = new MyObject( void 0 ); obj.substring(0, 100)",
                                    "undefined",
                                    (obj = new MyObject( void 0 ), obj.substring(0,100) ));

    return array;
}

function MyObject( value ) {
    this.value = value;
    this.substring = String.prototype.substring;
   this.toString = function() { return this.value+''; }
}

