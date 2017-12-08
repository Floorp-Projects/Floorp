/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 14 Mar 2001
 *
 * SUMMARY: Testing the [[Class]] property of native constructors.
 * See ECMA-262 Edition 3 13-Oct-1999, Section 8.6.2 re [[Class]] property.
 *
 * Same as class-001.js - but testing the constructors here, not
 * object instances.  Therefore we expect the [[Class]] property to
 * equal 'Function' in each case.
 *
 * The getJSClass() function we use is in a utility file, e.g. "shell.js"
 */
//-----------------------------------------------------------------------------
var i = 0;
var UBound = 0;
var BUGNUMBER = '(none)';
var summary = 'Testing the internal [[Class]] property of native constructors';
var statprefix = 'Current constructor is: ';
var status = ''; var statusList = [ ];
var actual = ''; var actualvalue = [ ];
var expect= ''; var expectedvalue = [ ];

/*
 * We set the expect variable each time only for readability.
 * We expect 'Function' every time; see discussion above -
 */
status = 'Object';
actual = getJSClass(Object);
expect = 'Function';
addThis();

status = 'Function';
actual = getJSClass(Function);
expect = 'Function';
addThis();

status = 'Array';
actual = getJSClass(Array);
expect = 'Function';
addThis();

status = 'String';
actual = getJSClass(String);
expect = 'Function';
addThis();

status = 'Boolean';
actual = getJSClass(Boolean);
expect = 'Function';
addThis();

status = 'Number';
actual = getJSClass(Number);
expect = 'Function';
addThis();

status = 'Date';
actual = getJSClass(Date);
expect = 'Function';
addThis();

status = 'RegExp';
actual = getJSClass(RegExp);
expect = 'Function';
addThis();

status = 'Error';
actual = getJSClass(Error);
expect = 'Function';
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
