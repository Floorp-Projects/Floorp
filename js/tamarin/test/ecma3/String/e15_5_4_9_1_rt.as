/* ***** BEGIN LICENSE BLOCK ***** 
 Version: MPL 1.1/GPL 2.0/LGPL 2.1 

The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
"License"); you may not use this file except in compliance with the License. You may obtain 
a copy of the License at http://www.mozilla.org/MPL/ 

Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
language governing rights and limitations under the License. 

The Original Code is [Open Source Virtual Machine.] 

The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
by the Initial Developer are Copyright (C)[ 2005-2006 ] Adobe Systems Incorporated. All Rights 
Reserved. 

Contributor(s): Adobe AS3 Team

Alternatively, the contents of this file may be used under the terms of either the GNU 
General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
LGPL are applicable instead of those above. If you wish to allow use of your version of this 
file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
version of this file under the terms of the MPL, indicate your decision by deleting provisions 
above and replace them with the notice and other provisions required by the GPL or the 
LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
under the terms of any one of the MPL, the GPL or the LGPL. 

 ***** END LICENSE BLOCK ***** */

    var SECTION = "15.5.4.9-1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "String.prototype.substring( start )";

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
                                    "var s = new String(''); s.substring()",
                                    "",
                                    (s = new String(''), s.substring() ) );


    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.substring()",
                                    "this is a string object",
                                    (s = new String('this is a string object'), s.substring() ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.substring(NaN)",
                                    "this is a string object",
                                    (s = new String('this is a string object'), s.substring(NaN) ) );


    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.substring(-0.01)",
                                    "this is a string object",
                                    (s = new String('this is a string object'), s.substring(-0.01) ) );


    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.substring(s.length)",
                                    "",
                                    (s = new String('this is a string object'), s.substring(s.length) ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.substring(s.length+1)",
                                    "",
                                    (s = new String('this is a string object'), s.substring(s.length+1) ) );


    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.substring(Infinity)",
                                    "",
                                    (s = new String('this is a string object'), s.substring(Infinity) ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.substring(-Infinity)",
                                    "this is a string object",
                                    (s = new String('this is a string object'), s.substring(-Infinity) ) );

    // this is not a String object, start is not an integer


    array[item++] = new TestCase(   SECTION,
                                    "var s = new Array(1,2,3,4,5); s.substring = String.prototype.substring; s.substring()",
                                    "1,2,3,4,5",
                                    (s = new Array(1,2,3,4,5), s.substring = String.prototype.substring, s.substring() ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new Array(1,2,3,4,5); s.substring = String.prototype.substring; s.substring(true)",
                                    ",2,3,4,5",
                                    (s = new Array(1,2,3,4,5), s.substring = String.prototype.substring, s.substring(true) ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new Array(1,2,3,4,5); s.substring = String.prototype.substring; s.substring('4')",
                                    "3,4,5",
                                    (s = new Array(1,2,3,4,5), s.substring = String.prototype.substring, s.substring('4') ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new Array(); s.substring = String.prototype.substring; s.substring('4')",
                                    "",
                                    (s = new Array(), s.substring = String.prototype.substring, s.substring('4') ) );

    // this is an object object
    array[item++] = new TestCase(   SECTION,
                                    "var obj = new Object(); obj.substring = String.prototype.substring; obj.substring(8)",
                                    "Object]",
                                    (obj = new Object(), obj.substring = String.prototype.substring, obj.substring(8) ) );

    // this is a function object

    array[item++] = new TestCase(   SECTION,
                                    "var obj = function() {}; obj.substring = String.prototype.substring; obj.toString = Object.prototype.toString; obj.substring(8)",
                                    true,
                                    (obj = function() {}, obj.substring = String.prototype.substring, obj.toString = Object.prototype.toString, obj.substring(8))=="Function-2]" ||
                                    (obj = function() {}, obj.substring = String.prototype.substring, obj.toString = Object.prototype.toString, obj.substring(8))=="null]"
                                     );
    // this is a number object

    thisError="no error";
    try{
        var obj = new Number(NaN);
        obj.substring = String.prototype.substring;
        obj.substring(false);
    }catch(e1:Error){
        thisError=e1.toString();
    }finally{
        array[item++] = new TestCase(   SECTION,
                                    "var obj = new Number(NaN); obj.substring = String.prototype.substring; obj.substring(false)",
                                    "ReferenceError: Error #1056",referenceError(thisError)
                                    );
    } 
   /* array[item++] = new TestCase(   SECTION,
                                    "var obj = new Number(NaN); obj.substring = String.prototype.substring; obj.substring(false)",
                                    "NaN",
                                    (obj = new Number(NaN), obj.substring = String.prototype.substring, obj.substring(false) ) );*/


    // this is the Math object
    array[item++] = new TestCase(   SECTION,
                                    "var obj = Math; obj.substring = String.prototype.substring; obj.substring(Math.PI)",
                                    "ass Math]",
                                    (obj = Math, obj.substring = String.prototype.substring, obj.substring(Math.PI) ) );

    // this is a Boolean object

    thisError="no error";
    try{
        var obj = new Boolean();
        obj.substring = String.prototype.substring;
        obj.substring(new Array());
    }catch(e3:Error){
        thisError=e3.toString();
    }finally{
        array[item++] = new TestCase(   SECTION,
                                    "var obj = new Boolean(); obj.substring = String.prototype.substring; obj.substring(new Array())",
                                    "ReferenceError: Error #1056",referenceError(thisError)
                                    );
    } 

    /*array[item++] = new TestCase(   SECTION,
                                    "var obj = new Boolean(); obj.substring = String.prototype.substring; obj.substring(new Array())",
                                    "false",
                                    (obj = new Boolean(), obj.substring = String.prototype.substring, obj.substring(new Array()) ) );*/

    // this is a user defined object



    array[item++] = new TestCase( SECTION,
                                    "var obj = new MyObject( null ); obj.substring(0)",
                                    "null",
                                    (obj = new MyObject( null ), obj.substring(0) ) );

    return array;
}
function MyObject( value ) {
    this.value = value;
    this.substring = String.prototype.substring;
    this.toString = function () { return this.value+'' }
}
