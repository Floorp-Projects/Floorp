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
* Date: 2001-06-27
*
* SUMMARY: Accessing static class methods via C or instances of C
*
*/
//-------------------------------------------------------------------------------------------------
var UBound = 0;
var bug = '(none)';
var summary = 'Accessing static class methods via C or instances of C';
var cnReturn = 'Return value of static method m of class C';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


class C
{
  static function m()
  {
    return cnReturn;
  }
}


var c1 = new C;
var c2 = new C;


status = inSection(1);
actual = C.m;
expect = c1.m;
addThis();

status = inSection(2);
actual = C.m;
expect = (new C).m;
addThis();

status = inSection(3);
actual = C.m();
expect = c1.m();
addThis();

status = inSection(4);
actual = C.m();
expect = (new C).m();
addThis();

status = inSection(5);
actual = c1.m;
expect = c2.m;
addThis();

status = inSection(6);
actual = c1.m();
expect = c2.m();
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
 
  for (var i = 0; i < UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
