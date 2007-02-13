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

    var SECTION = "15.5.4.8-1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "String.prototype.split";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION,  "String.prototype.split.length",        2,          String.prototype.split.length );
    array[item++] = new TestCase( SECTION,  "delete String.prototype.split.length", false,      delete String.prototype.split.length );
    array[item++] = new TestCase( SECTION,  "delete String.prototype.split.length; String.prototype.split.length", 2,      (delete String.prototype.split.length, String.prototype.split.length));

    // test cases for when split is called with no arguments.

    // this is a string object

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); typeof s.split()",
                                    "object",
                                    (s = new String('this is a string object'), typeof s.split() ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); Array.prototype.getClass = Object.prototype.toString; (s.split()).getClass()",
                                    "[object Array]",
                                    (s = new String('this is a string object'), Array.prototype.getClass = Object.prototype.toString, (s.split()).getClass() ) );

    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.split().length",
                                    1,
                                    (s = new String('this is a string object'), s.split().length ) );

 
    array[item++] = new TestCase(   SECTION,
                                    "var s = new String('this is a string object'); s.split()[0]",
                                    "this is a string object",
                                    (s = new String('this is a string object'), s.split()[0] ) );
 

    // this is an object object
    array[item++] = new TestCase(   SECTION,
                                    "var obj = new Object(); obj.split = String.prototype.split; typeof obj.split()",
                                    "object",
                                    (obj = new Object(), obj.split = String.prototype.split, typeof obj.split() ) );

    array[item++] = new TestCase(   SECTION,
                                    "var obj = new Object(); obj.split = String.prototype.split; Array.prototype.getClass = Object.prototype.toString; obj.getClass()",
                                    "[object Array]",
                                    (obj = new Object(), obj.split = String.prototype.split, Array.prototype.getClass = Object.prototype.toString, obj.split().getClass() ) );

    array[item++] = new TestCase(   SECTION,
                                    "var obj = new Object(); obj.split = String.prototype.split; obj.split().length",
                                    1,
                                    (obj = new Object(), obj.split = String.prototype.split, obj.split().length ) );

    array[item++] = new TestCase(   SECTION,
                                    "var obj = new Object(); obj.split = String.prototype.split; obj.split()[0]",
                                    "[object Object]",
                                    (obj = new Object(), obj.split = String.prototype.split, obj.split()[0] ) );
   
        

    // this is a function object
    array[item++] = new TestCase(   SECTION,
                                    "var obj = function() {}; obj.split = String.prototype.split; typeof obj.split()",
                                    "object",
                                    (obj = function() {}, obj.split = String.prototype.split, typeof obj.split() ) );

    array[item++] = new TestCase(   SECTION,
                                    "var obj = function() {}; obj.split = String.prototype.split; Array.prototype.getClass = Object.prototype.toString; obj.getClass()",
                                    "[object Array]",
                                    (obj = function() {}, obj.split = String.prototype.split, Array.prototype.getClass = Object.prototype.toString, obj.split().getClass() ) );

    array[item++] = new TestCase(   SECTION,
                                    "var obj = function() {}; obj.split = String.prototype.split; obj.split().length",
                                    1,
                                    (obj = function() {}, obj.split = String.prototype.split, obj.split().length ) );

   /* commenting out due to bug 175096
    array[item++] = new TestCase(   SECTION,
                                    "var obj = function() {}; obj.split = String.prototype.split; obj.toString = Object.prototype.toString; obj.split()[0]",
                                    "[object Function]",
                                    (obj = function() {}, obj.split = String.prototype.split, obj.toString = Object.prototype.toString, (obj.split()[0]).substring(0,16)+"]"));
    */

    // this is a number object

    thisError="no error";
    var obj = new Number(NaN);
    try{
    
        obj.split = String.prototype.split;
    
    }catch(e1:Error){
        thisError=e1.toString();
    }finally{//print(thisError);
        array[item++] = new TestCase(SECTION,"var obj = new Number(NaN); obj.split = String.prototype.split; typeof obj.split()",
"ReferenceError: Error #1056",referenceError(thisError));
    }



   /* array[item++] = new TestCase(   SECTION,
                                    "var obj = new Number(NaN); obj.split = String.prototype.split; typeof obj.split()",
                                    "object",
                                    (obj = new Number(NaN), obj.split = String.prototype.split, typeof obj.split() ) );*/

    thisError="no error";
    var obj = new Number(NaN);
    try{
    
        obj.split = String.prototype.split;
        Array.prototype.getClass = Object.prototype.toString;
        obj.getClass();
    }catch(e2:Error){
        thisError=e2.toString();
    }finally{//print(thisError);
        array[item++] = new TestCase(SECTION,"var obj = new Number(Infinity); obj.split = String.prototype.split; Array.prototype.getClass = Object.prototype.toString; obj.getClass()",
"ReferenceError: Error #1056",referenceError(thisError));
    }

   /* array[item++] = new TestCase(   SECTION,
                                    "var obj = new Number(Infinity); obj.split = String.prototype.split; Array.prototype.getClass = Object.prototype.toString; obj.getClass()",
                                    "[object Array]",
                                    (obj = new Number(Infinity), obj.split = String.prototype.split, Array.prototype.getClass = Object.prototype.toString, obj.split().getClass() ) );*/

    thisError="no error";
    var obj = new Number(-1234567890);
    try{
        obj.split = String.prototype.split;
        obj.split().length;
    }catch(e3:Error){
        thisError=e3.toString();
    }finally{//print(thisError);
        array[item++] = new TestCase(SECTION,"var obj = new Number(-1234567890); obj.split = String.prototype.split; obj.split().length","ReferenceError: Error #1056",referenceError(thisError));
    }


    /*array[item++] = new TestCase(   SECTION,
                                    "var obj = new Number(-1234567890); obj.split = String.prototype.split; obj.split().length",
                                    1,
                                    (obj = new Number(-1234567890), obj.split = String.prototype.split, obj.split().length ) );*/

    thisError="no error";
    var obj = new Number(-1e21);
    try{
        obj.split = String.prototype.split;
        obj.split()[0];
    }catch(e4:Error){
        thisError=e4.toString();
    }finally{//print(thisError);
        array[item++] = new TestCase(SECTION,"var obj = new Number(-1e21); obj.split = String.prototype.split; obj.split()[0]","ReferenceError: Error #1056",referenceError(thisError));
    }

    /*array[item++] = new TestCase(   SECTION,
                                    "var obj = new Number(-1e21); obj.split = String.prototype.split; obj.split()[0]",
                                    "-1e+21",
                                    (obj = new Number(-1e21), obj.split = String.prototype.split, obj.split()[0] ) );*/


    // this is the Math object

    array[item++] = new TestCase(   SECTION,
                                    "var obj = Math; obj.split = String.prototype.split; typeof obj.split()",
                                    "object",
                                    (obj = Math, obj.split = String.prototype.split, typeof obj.split() ) );

    thisError="no error";
    var obj = Math;
    try{
        obj.split = String.prototype.split;
        Array.prototype.getClass = Object.prototype.toString;
        obj.getClass();
    }catch(e6:Error){
        thisError=e6.toString();
    }finally{
        array[item++] = new TestCase(SECTION,"var obj = Math; obj.split = String.prototype.split;Array.prototype.getClass = Object.prototype.toString; obj.getClass()","TypeError: Error #1006",typeError(thisError));
    }

   /* array[item++] = new TestCase(   SECTION,
                                    "var obj = Math; obj.split = String.prototype.split; Array.prototype.getClass = Object.prototype.toString; obj.getClass()",
                                    "[object Array]",
                                    (obj = Math, obj.split = String.prototype.split, Array.prototype.getClass = Object.prototype.toString, obj.split().getClass() ) );*/

    array[item++] = new TestCase(   SECTION,
                                    "var obj = Math; obj.split = String.prototype.split; obj.split().length",
                                    1,
                                    (obj = Math, obj.split = String.prototype.split, obj.split().length ) );

    array[item++] = new TestCase(   SECTION,
                                    "var obj = Math; obj.split = String.prototype.split; obj.split()[0]",
                                    "[class Math]",
                                    (obj = Math, obj.split = String.prototype.split, obj.split()[0] ) );

    // this is an array object
    array[item++] = new TestCase(   SECTION,
                                    "var obj = new Array(1,2,3,4,5); obj.split = String.prototype.split; typeof obj.split()",
                                    "object",
                                    (obj = new Array(1,2,3,4,5), obj.split = String.prototype.split, typeof obj.split() ) );

    array[item++] = new TestCase(   SECTION,
                                    "var obj = new Array(1,2,3,4,5); obj.split = String.prototype.split; Array.prototype.getClass = Object.prototype.toString; obj.getClass()",
                                    "[object Array]",
                                    (obj = new Array(1,2,3,4,5), obj.split = String.prototype.split, Array.prototype.getClass = Object.prototype.toString, obj.split().getClass() ) );

    array[item++] = new TestCase(   SECTION,
                                    "var obj = new Array(1,2,3,4,5); obj.split = String.prototype.split; obj.split().length",
                                    1,
                                    (obj = new Array(1,2,3,4,5), obj.split = String.prototype.split, obj.split().length ) );

    array[item++] = new TestCase(   SECTION,
                                    "var obj = new Array(1,2,3,4,5); obj.split = String.prototype.split; obj.split()[0]",
                                    "1,2,3,4,5",
                                    (obj = new Array(1,2,3,4,5), obj.split = String.prototype.split, obj.split()[0] ) );

    // this is a Boolean object

    thisError="no error";
    var obj = new Boolean();
    try{
    
        obj.split = String.prototype.split;
    
    }catch(e9:Error){
        thisError=e9.toString();
    }finally{//print(thisError);
        array[item++] = new TestCase(SECTION,"var obj = new Boolean(); obj.split = String.prototype.split; typeof obj.split()",
"ReferenceError: Error #1056",referenceError(thisError));
    }



   /* array[item++] = new TestCase(   SECTION,
                                    "var obj = new Boolean(); obj.split = String.prototype.split; typeof obj.split()",
                                    "object",
                                    (obj = new Boolean(), obj.split = String.prototype.split, typeof obj.split() ) );*/

    thisError="no error";
    var obj = new Boolean();
    try{
    
        obj.split = String.prototype.split;
        Array.prototype.getClass = Object.prototype.toString;
        obj.getClass();
    
    }catch(e10:Error){
        thisError=e10.toString();
    }finally{//print(thisError);
        array[item++] = new TestCase(SECTION,"var obj = new Boolean(); obj.split = String.prototype.split; Array.prototype.getClass = Object.prototype.toString; obj.getClass()",
"ReferenceError: Error #1056",referenceError(thisError));
    }


   /* array[item++] = new TestCase(   SECTION,
                                    "var obj = new Boolean(); obj.split = String.prototype.split; Array.prototype.getClass = Object.prototype.toString; obj.getClass()",
                                    "[object Array]",
                                    (obj = new Boolean(), obj.split = String.prototype.split, Array.prototype.getClass = Object.prototype.toString, obj.split().getClass() ) );*/

    thisError="no error";
    var obj = new Boolean();
    try{
    
        obj.split = String.prototype.split;
        obj.split().length;
    
    
    }catch(e11:Error){
        thisError=e11.toString();
    }finally{//print(thisError);
        array[item++] = new TestCase(SECTION,"var obj = new Boolean(); obj.split = String.prototype.split;obj.split().length",
"ReferenceError: Error #1056",referenceError(thisError));
    }

   /* array[item++] = new TestCase(   SECTION,
                                    "var obj = new Boolean(); obj.split = String.prototype.split; obj.split().length",
                                    1,
                                    (obj = new Boolean(), obj.split = String.prototype.split, obj.split().length ) );*/

    thisError="no error";
    var obj = new Boolean();
    try{
    
        obj.split = String.prototype.split;
        obj.split()[0];
    
    
    }catch(e12:Error){
        thisError=e12.toString();
    }finally{//print(thisError);
        array[item++] = new TestCase(SECTION,"var obj = new Boolean(); obj.split = String.prototype.split;obj.split()[0]",
"ReferenceError: Error #1056",referenceError(thisError));
    }

    /*array[item++] = new TestCase(   SECTION,
                                    "var obj = new Boolean(); obj.split = String.prototype.split; obj.split()[0]",
                                    "false",
                                    (obj = new Boolean(), obj.split = String.prototype.split, obj.split()[0] ) );*/


    //delete extra property created
    delete Array.prototype.getClass;
    
    return array;
}
