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

    var SECTION = "12.2-1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "The variable statement";

    writeHeaderToLog( SECTION +" "+ TITLE);

    var testcases = getTestCases();
    
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;    

    var x = 3;
    function f() { var a = x; x = 23; return a; }

    array[item++] = new TestCase(    "SECTION",
                                    "var x = 3; function f() { var a = x; var x = 23; return a; }; f()",3,f() );

    array[item++] = new TestCase(    "SECTION",
                                    "var x created with global scope with property attribute {DontDelete}",false,delete x);

    array[item++] = new TestCase(    "SECTION",
                                    "var x created with global scope with property attribute {DontDelete}",23,x);
  
    function g(){
        var y = 20;
    }
    var thisError="no error";
    try{
        y++;
   
    }catch(e:ReferenceError){
        thisError=e.toString();
    }finally{
        array[item++] = new TestCase(    "SECTION",
                                    "var y created with function scope","ReferenceError: Error #1065",referenceError(thisError));
    }

    function h(){
        var i;
        return i;
    }
 
    array[item++] = new TestCase(    "SECTION",
                                    "undefined variables created with function scope",undefined,h());

    function MyFunction(){
        var myvar1:int=10,myvar2:int=20,myvar3:int;
        myvar3=myvar1+myvar2;
        return myvar3;
    }
 
    array[item++] = new TestCase(    "SECTION",
                                    "variables created with function scope and assigned with values",30,MyFunction());

   
    var l,m,n;

    array[item++] = new TestCase(    "SECTION",
                                    "undefined variables",undefined,l);

    array[item++] = new TestCase(    "SECTION",
                                    "undefined variables",undefined,m);

    array[item++] = new TestCase(    "SECTION",
                                    "undefined variables",undefined,n);
    var t;var u;var v;

    array[item++] = new TestCase(    "SECTION",
                                    "undefined variables",undefined,t);

    array[item++] = new TestCase(    "SECTION",
                                    "undefined variables",undefined,u);

    array[item++] = new TestCase(    "SECTION",
                                    "undefined variables",undefined,v);

    v=u;

    array[item++] = new TestCase(    "SECTION",
                                    "undefined variables",undefined,v);
    v=x;

    array[item++] = new TestCase(    "SECTION",
                                    "variable assigned with value when the variable statement is executed",23,v);

    var d:Number=100,b:Array=new Array(1,2,3),k:Boolean=true,g:String="string";

    array[item++] = new TestCase(    "SECTION",
                                    "variable assigned with value when the variable statement is executed",100,d);

    array[item++] = new TestCase(    "SECTION",
                                    "variable assigned with value when the variable statement is executed","1,2,3",b+"");

    array[item++] = new TestCase(    "SECTION",
                                    "variable assigned with value when the variable statement is executed",true,k);
   
    array[item++] = new TestCase(    "SECTION",
                                    "variable assigned with value when the variable statement is executed","string",g);
    return array;
}
