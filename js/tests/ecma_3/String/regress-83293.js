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
* Copyright (C) 1998 Netscape Communications Corporation. All
* Rights Reserved.
*
* Contributor(s): pschwartau@netscape.com
* Date: 30 May 2001
*
* SUMMARY:  Regression test for bug 83293
* See http://bugzilla.mozilla.org/show_bug.cgi?id=83293
*
* str.replace(strA, strB) == str.replace(new RegExp(strA),strB)
* See ECMA-262 Section 15.5.4.11 String.prototype.replace
*/
//-------------------------------------------------------------------------------------------------
var bug = 83293;
var summary = 'Testing str.replace(strA, strB) == str.replace(new RegExp(strA),strB)';
var status = '';
var actual = '';
var expect= '';
var cnEmptyString = '';
var str = 'abc';
var strA = '';
var strB = 'Z';


//-------------------------------------------------------------------------------------------------
test();
//-------------------------------------------------------------------------------------------------


function test()
{
  enterFunc ('test');
  printBugNumber (bug);
  printStatus (summary);

  status = 'Section A of test';
  strA = 'a';
  actual = str.replace(strA, strB);
  expect = str.replace(new RegExp(strA), strB);
  reportCompare(expect, actual, status);

  status = 'Section B of test';
  strA = 'x';
  actual = str.replace(strA, strB);
  expect = str.replace(new RegExp(strA), strB);
  reportCompare(expect, actual, status);

  status = 'Section C of test';
  strA = undefined;
  actual = str.replace(strA, strB);
  expect = str.replace(new RegExp(strA), strB);
  reportCompare(expect, actual, status);

  status = 'Section D of test';
  strA = null;
  actual = str.replace(strA, strB);
  expect = str.replace(new RegExp(strA), strB);
  reportCompare(expect, actual, status);

  status = 'Section E of test';
  strA = cnEmptyString;
  actual = str.replace(strA, strB);
  expect = str.replace(new RegExp(strA), strB);
  reportCompare(expect, actual, status);

  exitFunc ('test');
}
