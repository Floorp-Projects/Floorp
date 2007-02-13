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
    var SECTION = "11.1.5";
    var VERSION = "ECMA_4";
    startTest();
    var TITLE   = "Object Initialisers";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;
    MyObject = {}
    array[item++] = new TestCase( SECTION,  "MyObject = {}",    "[object Object]",  MyObject.toString() );

    array[item++] = new TestCase( SECTION,  "MyObject = {}, typeof MyObject","object",typeof MyObject );

    MyNumberObject = {MyNumber:10}

    array[item++] = new TestCase( SECTION,  "MyNumberObject = {MyNumber:10}",10,MyNumberObject.MyNumber );

    MyStringObject = {MyString:"string"}

    array[item++] = new TestCase( SECTION,  "MyStringObject = {MyString:string}","string",MyStringObject.MyString );

    MyBooleanObject = {MyBoolean:true}

    array[item++] = new TestCase( SECTION,  "MyBooleanObject = {MyBoolean:true}",true,MyBooleanObject.MyBoolean );

    function myfunc():String{
        return "Hi!!!"}

    MyObject2 = {MyNumber:10,MyString:'string',MyBoolean:true,myarr:[1,2,3],myfuncvar:myfunc}

    array[item++] = new TestCase( SECTION,  "MyObject2 = {MyNumber:10,MyString:'string',MyBoolean:true,myarr:[1,2,3],myfuncvar:myfunc}",10,MyObject2.MyNumber );

    

    array[item++] = new TestCase( SECTION,  "MyObject2 = {MyNumber:10,MyString:'string',MyBoolean:true,myarr:[1,2,3],myfuncvar:myfunc}","string",MyObject2.MyString );

    

    array[item++] = new TestCase( SECTION,  "MyObject2 = {MyNumber:10,MyString:'string',MyBoolean:true,myarr:[1,2,3],myfuncvar:myfunc}",true,MyObject2.MyBoolean );

    array[item++] = new TestCase( SECTION,  "MyObject2 = {MyNumber:10,MyString:'string',MyBoolean:true,myarr:[1,2,3],myfuncvar:myfunc}","number",typeof MyObject2.MyNumber );

    

    array[item++] = new TestCase( SECTION,  "MyObject2 = {MyNumber:10,MyString:'string',MyBoolean:true,myarr:[1,2,3],myfuncvar:myfunc}","string",typeof MyObject2.MyString );

    

    array[item++] = new TestCase( SECTION,  "MyObject2 = {MyNumber:10,MyString:'string',MyBoolean:true,myarr:[1,2,3],myfuncvar:myfunc}","boolean",typeof MyObject2.MyBoolean );

    array[item++] = new TestCase( SECTION,  "MyObject2 = {MyNumber:10,MyString:'string',MyBoolean:true,myarr:[1,2,3],myfuncvar:myfunc}",3,MyObject2.myarr.length );
    

   array[item++] = new TestCase( SECTION,  "MyObject2 = {MyNumber:10,MyString:'string',MyBoolean:true,myarr:[1,2,3],myfuncvar:myfunc}","object",typeof MyObject2.myarr );

   array[item++] = new TestCase( SECTION,  "MyObject2 = {MyNumber:10,MyString:'string',MyBoolean:true,myarr:[1,2,3],myfuncvar:myfunc}","Hi!!!",MyObject2.myfuncvar() );

  array[item++] = new TestCase( SECTION,  "MyObject2 = {MyNumber:10,MyString:'string',MyBoolean:true,myarr:[1,2,3],myfuncvar:myfunc}","function",typeof MyObject2.myfuncvar );
    
    
    MyObject3 = {myvar:this}

    array[item++] = new TestCase( SECTION,  "MyObject3 = {this}","object",typeof MyObject3.myvar);

    array[item++] = new TestCase( SECTION,  "MyObject3 = {this}","[object global]",MyObject3.myvar+"");

   MyObject4 = {myvar:function() {}}

   array[item++] = new TestCase( SECTION,  "MyObject4 = {myvar:function() {}}","function",typeof MyObject4.myvar);

   array[item++] = new TestCase( SECTION,  "MyObject4 = {myvar:function() {}}","function Function() {}",MyObject4.myvar+"");

   array[item++] = new TestCase( SECTION,  "MyObject4 = {myvar:function() {}}","function Function() {}",MyObject4.myvar.toString());

   

   
   return ( array );
}

function MyObject( value ) {
    this.value = function() {return this.value;}
    this.toString = function() {return this.value+'';}
}
