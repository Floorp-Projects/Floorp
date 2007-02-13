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


    var SECTION = "15.5.4.13";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "String.prototype.slice( start, end )";
    
    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;
    array[item++] = new TestCase( SECTION,  "String.prototype.slice.length",        2,          String.prototype.slice.length );
    array[item++] = new TestCase( SECTION,  "delete String.prototype.slice.length", false,      delete String.prototype.slice.length );
    array[item++] = new TestCase( SECTION,  "delete String.prototype.slice.length; String.prototype.slice.length", 2,      (delete String.prototype.slice.length, String.prototype.slice.length) );

    // test cases for when slice is called with no arguments.

    // this is a string object

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); typeof s.slice()",
                                    "string",
                                    (s = new String('this is a string object'), typeof s.slice() ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String(''); s.slice(1,0)",
                                    "",
                                    (s = new String(''), s.slice(1,0) ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.slice(true, false)",
                                    "t",
                                    (s = new String('this is a string object'), s.slice(false, true) ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.slice(NaN, Infinity)",
                                    "this is a string object",
                                    (s = new String('this is a string object'), s.slice(NaN, Infinity) ) );


    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.slice(Infinity, NaN)",
                                    "",
                                    (s = new String('this is a string object'), s.slice(Infinity, NaN) ) );


    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.slice(Infinity, Infinity)",
                                    "",
                                    (s = new String('this is a string object'), s.slice(Infinity, Infinity) ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.slice(-0.01, 0)",
                                    "",
                                    (s = new String('this is a string object'), s.slice(-0.01,0) ) );


    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.slice(s.length, s.length)",
                                    "",
                                    (s = new String('this is a string object'), s.slice(s.length, s.length) ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.slice(s.length+1, 0)",
                                    "",
                                    (s = new String('this is a string object'), s.slice(s.length+1, 0) ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.slice(-Infinity, -Infinity)",
                                    "",
                                    (s = new String('this is a string object'), s.slice(-Infinity, -Infinity) ) );

    


    // this is an object object
    array[item++] = new TestCase(   SECTION,
                                    "var obj = new Object(); obj.slice = String.prototype.slice; obj.slice(0,8)",
                                    "[object ",
                                    (obj = new Object(), obj.toString= Object.prototype.toString,obj.slice = String.prototype.slice, obj.slice(0,8) ) );

    array[item++] = new TestCase(   SECTION,
                                    "var obj = new Object(); obj.slice = String.prototype.slice; obj.slice(8,obj.toString().length)",
                                    "Object]",
                                    (obj = new Object(), obj.slice = String.prototype.slice, obj.slice(8, obj.toString().length) ) );


    // this is a function object
    array[item++] = new TestCase(   SECTION,
                                    "var obj = function() {}; obj.slice = Object.prototype.slice; obj.slice(8, Infinity)",
                                    " Function() {}",
                                    (obj = function() {}, obj.slice = String.prototype.slice,obj.slice(8,Infinity)+"" ) );

  


    // this is a user defined object
    array[item++] = new TestCase( SECTION,
                                    "var obj = new MyObject( void 0 ); obj.slice(0, 100)",
                                    "undefined",
                                    (obj = new MyObject( void 0 ), obj.slice(0,100) ));

    return array;
}
function MyObject( value ) {
    this.value = value;
    this.slice= String.prototype.slice;
    this.substring = String.prototype.substring;
    this.toString = function() { return this.value+''; }
}

