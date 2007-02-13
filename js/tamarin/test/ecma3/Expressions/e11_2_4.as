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

    var SECTION = "11_2_4";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Argument List";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    
    test();
    
  class MyClass{}    
function getTestCases() {
    var array = new Array();
    var item = 0;    

    //Empty argument list

    var arr = new Array();

    array[item++] = new TestCase( SECTION,
                                    "arr.length",
                                    0,
                                    arr.length );

    trace(arr);

    array[item++] = new TestCase( SECTION,
                                    "Returning empty list of value of arr",
                                    '',
                                    arr.toString() );


   function MyFunction():String{
       return "Hi!";
   }


    array[item++] = new TestCase( SECTION,
                                    "MyFunction",
                                    "Hi!",
                                    MyFunction() );



   //Argument list with more arguments

    var arr1 = new Array(1,2,3,4,5);

    array[item++] = new TestCase( SECTION,
                                    "arr1.length",
                                    5,
                                    arr1.length );

    function MyFunction2(a,b,c,d,e):Number{
    return a+b+c+d+e;
    }

    array[item++] = new TestCase( SECTION,
                                    "MyFunction2 with 5 arguments",
                                    15,
                                    MyFunction2(1,2,3,4,5));
   var myvar1:Number = 1;
   var myvar2:Number =10;
   function foo():Number{
   myvar1 = myvar1+myvar2;
   return myvar1;
   }

   function goo():Number{
   myvar2 = myvar2*10;
   return myvar2;
   }

   function MyFunction3(d,e,f):Number{
   return f;
   }

   array[item++] = new TestCase( SECTION,
                                    "MyFunction3 with 3 arguments",
                                    111,
                                    MyFunction3(foo(),goo(),myvar1+myvar2));

   var arr2 = new Array(foo(),goo(),myvar1+myvar2);

   array[item++] = new TestCase( SECTION,
                                    "arr2.length",
                                    3,
                                    arr2.length );

   array[item++] = new TestCase( SECTION,
                                    "arr2[0]",
                                    111,
                                    arr2[0] );

   array[item++] = new TestCase( SECTION,
                                    "arr2[1]",
                                    1000,
                                    arr2[1] );

   array[item++] = new TestCase( SECTION,
                                    "arr2[2]",
                                    1111,
                                    arr2[2] );


  var myclass = new MyClass();

  function MyFunction4(a,b,c,d,e,f):void{}

  array[item++] = new TestCase( SECTION,
                                    "MyFunction3 with 3 arguments",
                                    11111,
                                    MyFunction3(foo(),goo(),myvar1+myvar2));

  var arr3 = new Array(1,"string",foo(),[1,2,3],true);

  array[item++] = new TestCase( SECTION,
                                    "arr3.length",
                                    5,
                                    arr3.length );

  array[item++] = new TestCase( SECTION,
                                    "arr3[0]",
                                    1,
                                    arr3[0] );

   array[item++] = new TestCase( SECTION,
                                    "arr3[1]",
                                    "string",
                                    arr3[1] );

   array[item++] = new TestCase( SECTION,
                                    "arr3[2]",
                                    11111,
                                    arr3[2] );

   array[item++] = new TestCase( SECTION,
                                    "arr3[3]",
                                    "1,2,3",
                                    arr3[3]+"" );



   array[item++] = new TestCase( SECTION,
                                    "arr3[4]",
                                    true,
                                    arr3[4]);


   var k:Number;
   var l:String;
   var m:Array;
   var p:Number;
   var n:Boolean;
   var q:String;
   var r:Number;

   function MyFunction4(a,b,c,d,e,g,h):String{
   k=a;
   l=b;
   m=c;
   n=e;
   p=d;
   q=g;
   r=h;
   return "passed";
   }

   array[item++] = new TestCase( SECTION,
                                    "Function with arguments of different data types",
                                    "passed",
                                    MyFunction4(1,"string",[2,3,4],goo(),false,null,void));

   array[item++] = new TestCase( SECTION,
                                    "Function with arguments of different data types",
                                    1,
                                     k);

   array[item++] = new TestCase( SECTION,
                                    "Function with arguments of different data types",
                                    1,
                                     k);
   array[item++] = new TestCase( SECTION,
                                    "Function with arguments of different data types",
                                    100000,
                                    p);

   array[item++] = new TestCase( SECTION,
                                    "Function with arguments of different data types",
                                    false,
                                    n);

   array[item++] = new TestCase( SECTION,
                                    "Function with arguments of different data types",
                                    null,
                                    q);

   array[item++] = new TestCase( SECTION,
                                    "Function with arguments of different data types",
                                    NaN,
                                    r);
    return array;
}


function TestFunction() {
    return arguments;
}
