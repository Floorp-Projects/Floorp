/*
* The contents of this file are subject to the Netscape Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS
* IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
* implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is mozilla.org code.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation.  Portions created by Netscape are
* Copyright (C) 1998 Netscape Communications Corporation.
* All Rights Reserved.
*
* Contributor(s): pschwartau@netscape.com
* Date: 2001-06-19
*
* SUMMARY: Basic test of classes: defining/invoking a method
*
* No expected vs. actual values are checked in the test() function here.
* Test is designed only to see that properties can be defined and invoked.
*/
//-------------------------------------------------------------------------------------------------
var UBound = 0;
var bug = '(none)';
var summary = 'Basic test of classes: defining/invoking a method';
var cnHELLO = 'HELLO';



// First, let the method have no arguments -
//-------------------------------------------------------------------------------------------------
class C1 {function myMethod1(){}}
var c1 = new C1;
c1.myMethod1();

class C2 {var myMethod2 = function(){}}
var c2 = new C2;
c2.myMethod2();

// without the 'var', myMethod should end up on the global object -
class C3 {myMethod3 = function(){}}
var c3 = new C3;
myMethod3(); 

class C4 {var myMethod4 = function XYZ(){}}
var c4 = new C4;
c4.myMethod4();

// without the 'var', myMethod should end up on the global object -
class C5 {myMethod5 = function XYZ(){}}
var c5 = new C5;
myMethod5();
//-------------------------------------------------------------------------------------------------




// Let the method have one argument but do nothing with it -
//-------------------------------------------------------------------------------------------------
class C6 {function myMethod6(msg){}}
var c6 = new C6;
c6.myMethod6(cnHELLO);

class C7 {var myMethod7 = function(msg){}}
var c7 = new C7;
c7.myMethod7(cnHELLO);

// without the 'var', myMethod should end up on the global object -
class C8 {myMethod8 = function(msg){}}
var c8 = new C8;
myMethod8(cnHELLO);

class C9 {var myMethod9 = function XYZ(msg){}}
var c9 = new C9;
c9.myMethod9(cnHELLO);

// without the 'var', myMethod should end up on the global object -
class C10 {myMethod10 = function XYZ(msg){}}
var c10 = new C10;
myMethod10(cnHELLO);
//-------------------------------------------------------------------------------------------------




// Let the method have one argument and do something with it -
//-------------------------------------------------------------------------------------------------
class C11 {function myMethod11(msg){print(msg);}}
var c11 = new C11;
c11.myMethod11(cnHELLO);

class C12 {var myMethod12 = function(msg){print(msg);}}
var c12 = new C12;
c12.myMethod12(cnHELLO);

// without the 'var', myMethod should end up on the global object -
class C13 {myMethod13 = function(msg){print(msg);}}
var c13 = new C13;
myMethod13(cnHELLO);

class C14 {var myMethod14 = function XYZ(msg){print(msg);}}
var c14 = new C14;
c14.myMethod14(cnHELLO);

// without the 'var', myMethod should end up on the global object -
class C15 {myMethod15 = function XYZ(msg){print(msg);}}
var c15 = new C15;
myMethod15(cnHELLO);
//-----------------------------------------------------------------------------



/*
 * No values are checked, and nothing interesting happens below. The test 
 * is designed to see simply that properties can be defined and invoked. 
 */
//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


function test()
{
  enterFunc ('test');
  printBugNumber (bug);
  printStatus (summary);
  exitFunc ('test');
}
