/*
* The contents of this file are subject to the Netscape Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an
* "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either expressed
* or implied. See the License for the specific language governing
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
* Date: 2001-06-26
*
* SUMMARY: Testing that classes can see global variables
*/
//-------------------------------------------------------------------------------------------------
var UBound = 0;
var bug = '(none)';
var summary = 'Testing that classes can see global variables';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var self = this; // capture a reference to the global object
var objX = {};
var objY = {}


class A 
{
}

class B 
{
  function B() 
  {
    objX = objA;  // global variable defined below -
  }
}

status = inSection(1);
actual = objX;
expect = self;
addThis();


/******************************************************

class C 
{
  function changeX() 
  {
    objX = self;
  }
}

class CC extends C 
{
  var m = test;
}


class D
{
  function changeX() 
  {
    objX = objA;
  }
}

class E
{
  function E (obj:B)
  {
    objX = obj;
  }
}

******************************************************/

var objA = new A;
var objB = new B;

/******************************************************

var objC = new C;
var objCC = new CC;
var objD = new D;
var objE = new E(objB);


objB.changeX();
objC.changeX();
objCC.m();
objD.changeX();

******************************************************/

//-------------------------------------------------------------------------------------------------
test();
//-------------------------------------------------------------------------------------------------


function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  enterFunc ('test');
  printBugNumber (bug);
  printStatus (summary);
  exitFunc ('test');
}
