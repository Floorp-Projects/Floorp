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
* Date: 2001-06-25
*
* SUMMARY: Negative test: a class method may not change the value of this.
*/
//-------------------------------------------------------------------------------------------------
var bug = '(none)';
var summary = 'Negative test: a class method may not change the value of this';
var self = this; // capture a reference to the global object


class A
{
  function changeThis()
  {
    this = self;
  }
}

class AA extends A
{
  var m1:Function = changeThis;

  function m2()
  {
    return typeof changeThis;
  }

  function m3()
  {
    return typeof m1;
  }
}


var objA = new A;
var objAA = new AA;


status = inSection(1);
actual = typeof objAA.m1;
expect = TYPE_FUNCTION;
addThis();

status = inSection(2);
actual = objAA.m2();
expect = TYPE_FUNCTION;
addThis();

status = inSection(3);
actual = objAA.m3();
expect = TYPE_FUNCTION;
addThis();



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
