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
* str.replace(strA, strB) == str.replace(new RegExp(strA),strB)
* See http://bugzilla.mozilla.org/show_bug.cgi?id=83293
*/
//-------------------------------------------------------------------------------------------------
var UBound = 0;
var bug = 83293;
var summary = 'Testing str.replace(strA, strB) == str.replace(new RegExp(strA),strB)' ;
var status = '';
var statusitems = [ ];
var actual = '';
var actualvalues = [ ];
var expect= '';
var expectedvalues = [ ];
var cnEmptyString = '';
var str = 'abc';
var strA = '';
var strB = 'Z';


status = 'Section A of test';
strA = 'a';
actual = str.replace(strA, strB);
expect = str.replace(new RegExp(strA), strB);
addThis();

status = 'Section B of test';
strA = 'x';
actual = str.replace(strA, strB);
expect = str.replace(new RegExp(strA), strB);
addThis();

status = 'Section C of test';
strA = undefined;
actual = str.replace(strA, strB);
expect = str.replace(new RegExp(strA), strB);
addThis();

status = 'Section D of test';
strA = null;
actual = str.replace(strA, strB);
expect = str.replace(new RegExp(strA), strB);
addThis();

status = 'Section E of test';
strA = cnEmptyString;
actual = str.replace(strA, strB);
expect = str.replace(new RegExp(strA), strB);
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
