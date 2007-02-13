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

    var SECTION = "15.2.1.1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Object( value )";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    var NULL_OBJECT = Object(null);


           array[item++] = new TestCase( SECTION, "Object(null).valueOf()", NULL_OBJECT,NULL_OBJECT.valueOf());

           array[item++] = new TestCase( SECTION, "typeof Object(null)",       "object",               typeof (Object(null)) );

           array[item++] = new TestCase( SECTION, "Object(null).constructor.prototype", "[object Object]",Object(null).constructor.prototype+"");





   var UNDEFINED_OBJECT = Object( void 0 );

           array[item++] = new TestCase( SECTION, "Object(void 0).valueOf()", UNDEFINED_OBJECT,UNDEFINED_OBJECT.valueOf());



    array[item++] = new TestCase( SECTION, "typeof Object(void 0)",       "object",               typeof (Object(void 0)) );

           array[item++] = new TestCase( SECTION, "Object(void 0).constructor.prototype", "[object Object]",(Object(void 0)).constructor.prototype+"");


    var UNDEFINED_OBJECT2 = Object(undefined);

           array[item++] = new TestCase( SECTION, "Object(undefined).valueOf", UNDEFINED_OBJECT2,UNDEFINED_OBJECT2.valueOf());



    array[item++] = new TestCase( SECTION, "typeof Object(undefined)",       "object",               typeof (Object(undefined)) );

           array[item++] = new TestCase( SECTION, "Object(undefined).constructor.prototype", "[object Object]",(Object(undefined)).constructor.prototype+"");


    array[item++] = new TestCase( SECTION, "Object(true).valueOf()",    true,                   (Object(true)).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object(true)",       "boolean",               typeof Object(true) );
    thisError = "no error";
    try{
       var MYOB = Object(true);
       MYOB.toString = Object.prototype.toString;
       MYOB.toString()
       }catch(e4:Error){
           thisError=e4.toString();
       }finally{
           array[item++] = new TestCase( SECTION, "var MYOB = Object(true), MYOB.toString = Object.prototype.toString, MYOB.toString()", "ReferenceError: Error #1056",referenceError(thisError));
        }


  array[item++] = new TestCase( SECTION, "Object(false).valueOf()",    false,                  (Object(false)).valueOf() );
  array[item++] = new TestCase( SECTION, "typeof Object(false)",      "boolean",               typeof Object(false) );


  thisError = "no error";
    try{
       var MYOB = Object(false);
       MYOB.toString = Object.prototype.toString;
       MYOB.toString()
       }catch(e5:Error){
           thisError=e5.toString();
       }finally{
           array[item++] = new TestCase( SECTION, "var MYOB = Object(false), MYOB.toString = Object.prototype.toString, MYOB.toString()", "ReferenceError: Error #1056",referenceError(thisError));
        }


  array[item++] = new TestCase( SECTION, "Object(0).valueOf()",       0,                      (Object(0)).valueOf() );
  array[item++] = new TestCase( SECTION, "typeof Object(0)",          "number",               typeof Object(0) );
  thisError = "no error";
    try{
       var MYOB = Object(0);
       MYOB.toString = Object.prototype.toString;
       MYOB.toString()
       }catch(e6:Error){
           thisError=e6.toString();
       }finally{
           array[item++] = new TestCase( SECTION, "var MYOB = Object(0), MYOB.toString = Object.prototype.toString, MYOB.toString()", "ReferenceError: Error #1056",referenceError(thisError));
        }




  array[item++] = new TestCase( SECTION, "Object(-0).valueOf()",      -0,                     (Object(-0)).valueOf() );
  array[item++] = new TestCase( SECTION, "typeof Object(-0)",         "number",               typeof Object(-0) );


  thisError = "no error";
    try{
       var MYOB = Object(-0);
       MYOB.toString = Object.prototype.toString;
       MYOB.toString()
       }catch(e7:Error){
           thisError=e7.toString();
       }finally{
           array[item++] = new TestCase( SECTION, "var MYOB = Object(-0), MYOB.toString", "ReferenceError: Error #1056",referenceError(thisError));
        }


  array[item++] = new TestCase( SECTION, "Object(1).valueOf()",       1,                      (Object(1)).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object(1)",          "number",               typeof Object(1) );
   thisError = "no error";
    try{
       var MYOB = Object(1);
       MYOB.toString = Object.prototype.toString;
       MYOB.toString()
       }catch(e8:Error){
           thisError=e8.toString();
       }finally{
           array[item++] = new TestCase( SECTION, "var MYOB = Object(1), MYOB.toString", "ReferenceError: Error #1056",referenceError(thisError));
        }



    array[item++] = new TestCase( SECTION, "Object(-1).valueOf()",      -1,                     (Object(-1)).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object(-1)",         "number",               typeof Object(-1) );

    thisError = "no error";
    try{
       var MYOB = Object(-1);
       MYOB.toString = Object.prototype.toString;
       MYOB.toString()
       }catch(e9:Error){
           thisError=e9.toString();
       }finally{
           array[item++] = new TestCase( SECTION, "var MYOB = Object(-1), MYOB.toString", "ReferenceError: Error #1056",referenceError(thisError));
        }


    array[item++] = new TestCase( SECTION, "Object(Number.MAX_VALUE).valueOf()",    1.7976931348623157e308,         (Object(Number.MAX_VALUE)).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object(Number.MAX_VALUE)",       "number", typeof Object(Number.MAX_VALUE) );

    thisError = "no error";
    try{
       var MYOB = Object(Number.MAX_VALUE);
       MYOB.toString = Object.prototype.toString;
       MYOB.toString()
       }catch(e10:Error){
           thisError=e10.toString();
       }finally{
           array[item++] = new TestCase( SECTION, "var MYOB = Object(Number.MAX_VALUE), MYOB.toString", "ReferenceError: Error #1056",referenceError(thisError));
        }

    array[item++] = new TestCase( SECTION, "Object(Number.MIN_VALUE).valueOf()",     5e-324,           (Object(Number.MIN_VALUE)).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object(Number.MIN_VALUE)",       "number",         typeof Object(Number.MIN_VALUE) );

    thisError = "no error";
    try{
       var MYOB = Object(Number.MIN_VALUE);
       MYOB.toString = Object.prototype.toString;
       MYOB.toString()
       }catch(e11:Error){
           thisError=e11.toString();
       }finally{
           array[item++] = new TestCase( SECTION, "var MYOB = Object(Number.MIN_VALUE), MYOB.toString", "ReferenceError: Error #1056",referenceError(thisError));
        }


    array[item++] = new TestCase( SECTION, "Object(Number.POSITIVE_INFINITY).valueOf()",    Number.POSITIVE_INFINITY,       (Object(Number.POSITIVE_INFINITY)).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object(Number.POSITIVE_INFINITY)",       "number",                       typeof Object(Number.POSITIVE_INFINITY) );

    thisError = "no error";
    try{
       var MYOB = Object(Number.POSITIVE_INFINITY);
       MYOB.toString = Object.prototype.toString;
       MYOB.toString()
       }catch(e12:Error){
           thisError=e12.toString();
       }finally{
           array[item++] = new TestCase( SECTION, "var MYOB = Object(Number.POSITIVE_INFINITY), MYOB.toString", "ReferenceError: Error #1056",referenceError(thisError));
        }


    array[item++] = new TestCase( SECTION, "Object(Number.NEGATIVE_INFINITY).valueOf()",    Number.NEGATIVE_INFINITY,       (Object(Number.NEGATIVE_INFINITY)).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object(Number.NEGATIVE_INFINITY)",       "number",            typeof Object(Number.NEGATIVE_INFINITY) );

    thisError = "no error";
    try{
       var MYOB = Object(Number.NEGATIVE_INFINITY);
       MYOB.toString = Object.prototype.toString;
       MYOB.toString()
       }catch(e13:Error){
           thisError=e13.toString();
       }finally{
           array[item++] = new TestCase( SECTION, "var MYOB = Object(Number.NEGATIVE_INFINITY), MYOB.toString", "ReferenceError: Error #1056",referenceError(thisError));
        }

  array[item++] = new TestCase( SECTION, "Object(Number.NaN).valueOf()",      Number.NaN,                (Object(Number.NaN)).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object(Number.NaN)",         "number",                  typeof Object(Number.NaN) );
    thisError = "no error";
    try{
       var MYOB = Object(Number.NaN);
       MYOB.toString = Object.prototype.toString;
       MYOB.toString()
       }catch(e14:Error){
           thisError=e14.toString();
       }finally{
           array[item++] = new TestCase( SECTION, "var MYOB = Object(Number.NaN), MYOB.toString", "ReferenceError: Error #1056",referenceError(thisError));
        }



    array[item++] = new TestCase( SECTION, "Object('a string').valueOf()",      "a string",         (Object("a string")).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object('a string')",         "string",           typeof (Object("a string")) );

    thisError = "no error";
    try{
       var MYOB = Object('a string');
       MYOB.toString = Object.prototype.toString;
       MYOB.toString()
       }catch(e15:Error){
           thisError=e15.toString();
       }finally{
         array[item++] = new TestCase( SECTION, "var MYOB = Object('a string'), MYOB.toString", "ReferenceError: Error #1056",referenceError(thisError));
        }

    array[item++] = new TestCase( SECTION, "Object('').valueOf()",              "",                 (Object("")).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object('')",                 "string",           typeof (Object("")) );

    thisError = "no error";
    try{
       var MYOB = Object('');
       MYOB.toString = Object.prototype.toString;
       MYOB.toString()
       }catch(e16:Error){
           thisError=e16.toString();
       }finally{
           array[item++] = new TestCase( SECTION, "var MYOB = Object(''), MYOB.toString", "ReferenceError: Error #1056",referenceError(thisError));
        }


    array[item++] = new TestCase( SECTION, "Object('\\r\\t\\b\\n\\v\\f').valueOf()",   "\r\t\b\n\v\f",   (Object("\r\t\b\n\v\f")).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object('\\r\\t\\b\\n\\v\\f')",      "string",           typeof (Object("\\r\\t\\b\\n\\v\\f")) );

    thisError = "no error";
    try{
       var MYOB = Object('\\r\\t\\b\\n\\v\\f');
       MYOB.toString = Object.prototype.toString;
       MYOB.toString()
       }catch(e17:Error){
           thisError=e17.toString();
       }finally{
           array[item++] = new TestCase( SECTION, "var MYOB = Object('\\r\\t\\b\\n\\v\\f'), MYOB.toString", "ReferenceError: Error #1056",referenceError(thisError));
        }


    array[item++] = new TestCase( SECTION,  "Object( '\\\'\\\"\\' ).valueOf()",      "\'\"\\",          (Object("\'\"\\")).valueOf() );
    array[item++] = new TestCase( SECTION,  "typeof Object( '\\\'\\\"\\' )",        "string",           typeof Object("\'\"\\") );

    var MYOB = Object('\\\'\\\"\\' );
    thisError = "no error";
    try{

       MYOB.toString = Object.prototype.toString;
       MYOB.toString()
       }catch(e18:Error){
           thisError=e18.toString();
       }finally{
           array[item++] = new TestCase( SECTION, "var MYOB = Object('\\\'\\\"\\' ), MYOB.toString", "ReferenceError: Error #1056",referenceError(thisError));
        }





    return ( array );
}
