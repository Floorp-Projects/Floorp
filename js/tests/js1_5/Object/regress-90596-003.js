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
* Date: 28 August 2001
*
* SUMMARY: A [DontEnum] prop, if overridden, should appear in for-in loops.
* See http://bugzilla.mozilla.org/show_bug.cgi?id=90596
*/
//-----------------------------------------------------------------------------
var UBound = 0;
var bug = 90596;
var summary = '[DontEnum] props (if overridden) should appear in for-in loops';
var cnCOMMA = ',';
var cnCOLON = ':';
var cnEMPTY = '';
var cnLBRACE = '{';
var cnRBRACE = '}';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var obj = {};
var s = '';


status = inSection(1);
obj = {toString:9};
actual = enumerateThis(obj);
expect = '{toString:9}';
addThis();

status = inSection(2);
obj = {hasOwnProperty:"Hi"};
actual = enumerateThis(obj);
expect = '{hasOwnProperty:"Hi"}';
addThis();

status = inSection(3);
obj = {toString:9, hasOwnProperty:"Hi"};
actual = enumerateThis(obj);
expect = '{toString:9, hasOwnProperty:"Hi"}';
addThis();

status = inSection(4);
obj = {prop1:1, toString:9, hasOwnProperty:"Hi"};
actual = enumerateThis(obj);
expect = '{prop1:1, toString:9, hasOwnProperty:"Hi"}';
addThis();


// TRY THE SAME THING IN EVAL CODE
status = inSection(5);
s = 'obj = {toString:9}';
eval(s);
actual = enumerateThis(obj);
expect = '{toString:9}';
addThis();

status = inSection(6);
s = 'obj = {hasOwnProperty:"Hi"}';
eval(s);
actual = enumerateThis(obj);
expect = '{hasOwnProperty:"Hi"}';
addThis();

status = inSection(7);
s = 'obj = {toString:9, hasOwnProperty:"Hi"}';
eval(s);
actual = enumerateThis(obj);
expect = '{toString:9, hasOwnProperty:"Hi"}';
addThis();

status = inSection(8);
s = 'obj = {prop1:1, toString:9, hasOwnProperty:"Hi"}';
eval(s);
actual = enumerateThis(obj);
expect = '{prop1:1, toString:9, hasOwnProperty:"Hi"}';
addThis();


// TRY THE SAME THING IN FUNCTION CODE
function A()
{
  status = inSection(9);
  var s = 'obj = {toString:9}';
  eval(s);
  actual = enumerateThis(obj);
  expect = '{toString:9}';
  addThis();
}
A();

function B()
{
  status = inSection(10);
  var s = 'obj = {hasOwnProperty:"Hi"}';
  eval(s);
  actual = enumerateThis(obj);
  expect = '{hasOwnProperty:"Hi"}';
  addThis();
}
B();

function C()
{
  status = inSection(11);
  var s = 'obj = {toString:9, hasOwnProperty:"Hi"}';
  eval(s);
  actual = enumerateThis(obj);
  expect = '{toString:9, hasOwnProperty:"Hi"}';
  addThis();
}
C();

function D()
{
  status = inSection(12);
  var s = 'obj = {prop1:1, toString:9, hasOwnProperty:"Hi"}';
  eval(s);
  actual = enumerateThis(obj);
  expect = '{prop1:1, toString:9, hasOwnProperty:"Hi"}';
  addThis();
}
D();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function enumerateThis(obj)
{
  var ret = '';

  for (var prop in obj)
  {
    ret += prop + cnCOLON + obj[prop] + cnCOMMA;
  }

  ret = stripLastComma(ret);
  ret = addBraces(ret);
  return ret;
}


function stripLastComma(text)
{
  return text.replace(/,$/, cnEMPTY);
}


function addBraces(text)
{
  return cnLBRACE + text + cnRBRACE;
}


function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = stripSpaces(actual);
  expectedvalues[UBound] = stripSpaces(expect);
  UBound++;
}


function stripSpaces(text)
{
  var ret = '';
  for (var i=0; i<text.length; i++)
  {
    if (!isWhiteSpace(text.charCodeAt(i)))
    {
      ret += text.charAt(i);
    }
  }
  return ret;
}


function isWhiteSpace(charCode)
{
  switch (charCode)
  {
    case (0x0009):
    case (0x000B):
    case (0x000C):
    case (0x0020):
    case (0x000A):  // '\n'
    case (0x000D):  // '\r'
      return true;
      break;

    default:
      return false;
  }
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
