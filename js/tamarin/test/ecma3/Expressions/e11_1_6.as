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
    var SECTION = "11.1.6";
    var VERSION = "ECMA_4";
    startTest();
    var TITLE   = "The Grouping Operator";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;
   
   array[item++] = new TestCase( SECTION,  "typeof(new Object())","object",typeof(new Object()));

   array[item++] = new TestCase( SECTION,  "typeof(new Array())","object",typeof(new Array()));

   array[item++] = new TestCase( SECTION,  "typeof(new Date())","object",typeof(new Date()));

   array[item++] = new TestCase( SECTION,  "typeof(new Boolean())","boolean",typeof(new Boolean()));

   array[item++] = new TestCase( SECTION,  "typeof(new String())","string",typeof(new String()));

   array[item++] = new TestCase( SECTION,  "typeof(new Number())","number",typeof(new Number()));

   array[item++] = new TestCase( SECTION,  "typeof(Math)","object",typeof(Math));

   array[item++] = new TestCase( SECTION,  "typeof(function(){})","function",typeof(function(){}));

   array[item++] = new TestCase( SECTION,  "typeof(this)","object",typeof(this));

   var MyVar:Number=10;

   array[item++] = new TestCase( SECTION,  "typeof(MyVar)","number",typeof(MyVar));

   array[item++] = new TestCase( SECTION,  "typeof([])","object",typeof([]));

   array[item++] = new TestCase( SECTION,  "typeof({})","object",typeof({}));

   array[item++] = new TestCase( SECTION,  "typeof('')","string",typeof(''));

   var MyArray = new Array(1,2,3);

   delete(MyArray[0])

   array[item++] = new TestCase( SECTION,  "delete(MyArray[0]);MyArray[0]",undefined,MyArray[0]);

   Number.prototype.foo=10;
   
   delete(Number.prototype.foo);

   array[item++] = new TestCase( SECTION,  "delete(Number.prototype.foo);",undefined,Number.prototype.foo);

   String.prototype.goo = 'hi';

   delete(String.prototype.goo);

   array[item++] = new TestCase( SECTION,  "delete(String.prototype.goo);",undefined,String.prototype.goo);

   Date.prototype.mydate=new Date(0);

   delete(Date.prototype.mydate);

   array[item++] = new TestCase( SECTION,  "delete(Date.prototype.mydate);",undefined,Date.prototype.mydate);

   

  array[item++] = new TestCase( SECTION,  "delete(new String('hi'));",true,delete(new String('hi')));

  array[item++] = new TestCase( SECTION,  "delete(new Date(0));",true,delete(new Date(0)));

  array[item++] = new TestCase( SECTION,  "delete(new Number(10));",true,delete(new Number(10)));

  array[item++] = new TestCase( SECTION,  "delete(new Object());",true,delete(new Object()));

  var obj = new Object();

  array[item++] = new TestCase( SECTION,  "delete(obj)  Trying to delete an object of reference type should return false",false,delete(obj));

  var a:Number = 10;
  var b:Number = 20;
  var c:Number = 30;
  var d:Number = 40;

  /*Grouping operators are used to change the normal hierarchy of the mathematical operators, expressions inside paranthesis are calculated first before any other expressions are calculated*/

  array[item++] = new TestCase( SECTION,  "Grouping operator used in defining the hierarchy of the operators",true,(a+b*c+d)!=((a+b)*(c+d)));

//Grouping operators are used when passing parameters through a function

  function myfunction(a):Number{
     return a*a;
  }

  array[item++] = new TestCase( SECTION,  "Grouping operator used in passing parameters to a function",4,myfunction(2));

  var a:Number = 1; 
  var b:Number = 2; 
  function foo() { a += b; } 
  function bar() { b *= 10; } 

  array[item++] = new TestCase( SECTION,  "Grouping operator used in evaluating functions and returning the results of an expression",23,(foo(), bar(), a + b));

  
 
  
  


  

   
      return ( array );
}

function MyObject( value ) {
    this.value = function() {return this.value;}
    this.toString = function() {return this.value+'';}
}
