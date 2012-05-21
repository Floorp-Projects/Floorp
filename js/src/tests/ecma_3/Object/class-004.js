/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 14 Mar 2001
 *
 * SUMMARY: Testing [[Class]] property of native error constructors.
 * See ECMA-262 Edition 3, Section 8.6.2 for the [[Class]] property.
 *
 * See ECMA-262 Edition 3, Section 15.11.6 for the native error types.
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=56868
 *
 * Same as class-003.js - but testing the constructors here, not
 * object instances.  Therefore we expect the [[Class]] property to
 * equal 'Function' in each case.
 *
 * The getJSClass() function we use is in a utility file, e.g. "shell.js"
 */
//-----------------------------------------------------------------------------
var i = 0;
var UBound = 0;
var BUGNUMBER = 56868;
var summary = 'Testing the internal [[Class]] property of native error constructors';
var statprefix = 'Current constructor is: ';
var status = ''; var statusList = [ ];
var actual = ''; var actualvalue = [ ];
var expect= ''; var expectedvalue = [ ];

/*
 * We set the expect variable each time only for readability.
 * We expect 'Function' every time; see discussion above -
 */
status = 'Error';
actual = getJSClass(Error);
expect = 'Function';
addThis();

status = 'EvalError';
actual = getJSClass(EvalError);
expect = 'Function';
addThis();

status = 'RangeError';
actual = getJSClass(RangeError);
expect = 'Function';
addThis();

status = 'ReferenceError';
actual = getJSClass(ReferenceError);
expect = 'Function';
addThis();

status = 'SyntaxError';
actual = getJSClass(SyntaxError);
expect = 'Function';
addThis();

status = 'TypeError';
actual = getJSClass(TypeError);
expect = 'Function';
addThis();

status = 'URIError';
actual = getJSClass(URIError);
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
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (i = 0; i < UBound; i++)
  {
    reportCompare(expectedvalue[i], actualvalue[i], getStatus(i));
  }

  exitFunc ('test');
}


function getStatus(i)
{
  return statprefix + statusList[i];
}
