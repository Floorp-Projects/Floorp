/*
* The contents of this file are subject to the Netscape Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS IS" 
* basis, WITHOUT WARRANTY OF ANY KIND, either expressed
* or implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is mozilla.org code.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation.  Portions created by Netscape are
* Copyright (C) 1998 Netscape Communications Corporation. All
* Rights Reserved.
*
* Contributor(s): pschwartau@netscape.com
* Date: 01 May 2001
*
* SUMMARY: Regression test for Bugzilla bug 74474
*"switch() misbehaves with duplicated labels"
*
* See ECMA3  Section 12.11,  "The switch Statement"
*/
//-------------------------------------------------------------------------------------------------
var UBound = 0;
var bug = '74474';
var summary = 'Testing switch statements with duplicate labels';
var status = '';
var statusList = [ ];
var actual = '';
var actualvalue = [ ];
var expect= '';
var expectedvalue = [ ];


// Section A: the string literal '1' as a duplicate label
actual = '';
switch ('1')
{
  case '1':
    actual += 'a';
  case '1':
    actual += 'b';
}
status = 'Section A: the string literal "1" as a duplicate label';
expect = 'ab';
addThis();


//  Section B: the numeric literal 1 as a duplicate label
actual = '';
switch (1)
{
  case 1:
    actual += 'a';
  case 1:
    actual += 'b';
}
status = 'Section B: the numeric literal 1 as a duplicate label';
expect = 'ab';
addThis();


//  Section C: the numeric literal 1 as a duplicate label, via a function parameter
tryThis(1);
function tryThis(x)
{
  actual = '';

  switch (x)
  {
    case x:
      actual += 'a';
    case x:
      actual += 'b';
  }
}
status = 'Section C: the numeric literal 1 as a duplicate label, via a function parameter';
expect = 'ab';
addThis();


//---------------------------------------------------------------------------------
test();
//---------------------------------------------------------------------------------


function addThis()
{
  statusList[UBound] = status;
  actualvalue[UBound] = actual;
  expectedvalue[UBound] = expect;
  UBound++;
}


function test()
{
  enterFunc ('test');
  printBugNumber (bug);
  printStatus (summary);
 
  for (var i = 0; i < UBound; i++)
  {
    reportCompare(expectedvalue[i], actualvalue[i], getStatus(i));
  }

  exitFunc ('test');
}


function getStatus(i)
{
  return statusList[i];
}
