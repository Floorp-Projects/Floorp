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
*          Will compare objX, or objX.prop, to expected values...
*/
//-----------------------------------------------------------------------------
var UBound:Integer = 0;
var bug:String = '(none)';
var summary:String = 'Testing that classes can see global variables';
var status:String = '';
var statusitems:Array = [];
var actual:String = '';
var actualvalues:Array = [];
var expect:String= '';
var expectedvalues:Array = [];
var cnA:String = 'A';
var cnC:String = 'C';
var cnX:String = 'X';
var cnY:String = 'Y';
var cnZ:String = 'Z';
var self:Object = this;
var objX:Object = {prop:cnX};
var objY:Object = {prop:cnY};


class A
{
  var prop:String;

  prototype function A()
  {
    prop = cnA;
    objX = objY;
  }
}


class B
{
  prototype function B()
  {
    // objA is defined below, but BEFORE any instances of B are made -
    objX = objA;
  }
}


class C
{
  var prop:String;

  prototype function C()
  {
    prop = cnC;
  }

  function changeX() 
  {
    objX = self;
  }
}


// Here is a CLASS-level "objX" to resolve...
class CC extends C
{
  var objX:Object;

  prototype function CC()
  {
    objX = this;
  }
}


class D
{
  prototype function D()
  {
  }

  static function changeX()
  {
    objX.prop = cnZ;
  }
}


class E
{
  prototype function E (obj:C)
  {
    objX= obj;
  }

  function changeX()
  {
    objX = new A;
  }
}



status = inSection(1);
actual = objX.prop;
expect = cnX;
addThis();

status = inSection(2);
var objA = new A;
actual = objX.prop;
expect = cnY;
addThis();

status = inSection(3);
var objB = new B;
actual = objX;
expect = objA;
addThis();

status = inSection(4);
var objC = new C;
actual = objX.prop;
expect = cnA;
addThis();

status = inSection(5);
objC.changeX();
actual = objX.prop;
expect = undefined;
addThis();

status = inSection(6);
var objCC = new CC;
actual = objX.prop;
expect = cnC; // expect objX == objCC, which inherits .prop from C
addThis();

status = inSection(7);
D.changeX(); // static method
actual = objX.prop;
expect = cnZ;
addThis();

status = inSection(8);
var objE = new E(objC);
actual = objX.prop;
expect = cnC;
addThis();

status = inSection(9);
objE.changeX();
actual = objX.prop;
expect = cnA;
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------


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

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
