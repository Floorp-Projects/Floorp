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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   pschwartau@netscape.com
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
/*
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

  constructor function A()
  {
    prop = cnA;
    objX = objY;
  }
}


class B
{
  constructor function B()
  {
    // objA is a global variable defined below -
    objX = objA;
  }
}


class C
{
  var prop:String;

  constructor function C()
  {
    prop = cnC;
  }

  function changeX() 
  {
    objX = self;
  }
}


class CC extends C
{
  constructor function CC()
  {
    objX = this;
  }
}


class D
{
  constructor function D()
  {
  }

  static function changeX()
  {
    objX.prop = cnZ;
  }
}


class E
{
  constructor function E (obj:C)
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
