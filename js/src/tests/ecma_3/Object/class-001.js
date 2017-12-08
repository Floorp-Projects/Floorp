/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 14 Mar 2001
 *
 * SUMMARY: Testing the internal [[Class]] property of objects
 * See ECMA-262 Edition 3 13-Oct-1999, Section 8.6.2
 *
 * The getJSClass() function we use is in a utility file, e.g. "shell.js".
 */
//-----------------------------------------------------------------------------
var i = 0;
var UBound = 0;
var BUGNUMBER = '(none)';
var summary = 'Testing the internal [[Class]] property of objects';
var statprefix = 'Current object is: ';
var status = ''; var statusList = [ ];
var actual = ''; var actualvalue = [ ];
var expect= ''; var expectedvalue = [ ];


status = 'the global object';
actual = getJSClass(this);
expect = GLOBAL;
if (expect == 'Window' && actual == 'XPCCrossOriginWrapper')
{
  print('Skipping global object due to XPCCrossOriginWrapper. See bug 390946');
}
else
{
  addThis();
}

status = 'new Object()';
actual = getJSClass(new Object());
expect = 'Object';
addThis();

status = 'new Function()';
actual = getJSClass(new Function());
expect = 'Function';
addThis();

status = 'new Array()';
actual = getJSClass(new Array());
expect = 'Array';
addThis();

status = 'new String()';
actual = getJSClass(new String());
expect = 'String';
addThis();

status = 'new Boolean()';
actual = getJSClass(new Boolean());
expect = 'Boolean';
addThis();

status = 'new Number()';
actual = getJSClass(new Number());
expect = 'Number';
addThis();

status = 'Math';
actual = getJSClass(Math);  // can't use 'new' with the Math object (EMCA3, 15.8)
expect = 'Math';
addThis();

status = 'new Date()';
actual = getJSClass(new Date());
expect = 'Date';
addThis();

status = 'new RegExp()';
actual = getJSClass(new RegExp());
expect = 'RegExp';
addThis();

status = 'new Error()';
actual = getJSClass(new Error());
expect = 'Error';
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
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (i = 0; i < UBound; i++)
  {
    reportCompare(expectedvalue[i], actualvalue[i], getStatus(i));
  }
}


function getStatus(i)
{
  return statprefix + statusList[i];
}
