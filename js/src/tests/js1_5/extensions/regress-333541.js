/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 333541;
var summary = '1..toSource()';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
function a(){
  return 1..toSource();
}

try
{
  expect = 'function a() {\n    return 1..toSource();\n}';
  actual = a.toString();
  compareSource(expect, actual, summary + ': 1');
}
catch(ex)
{
  actual = ex + '';
  reportCompare(expect, actual, summary + ': 1');
}

try
{
  expect = 'function a() {return 1..toSource();}';
  actual = a.toSource();
  compareSource(expect, actual, summary + ': 2');
}
catch(ex)
{
  actual = ex + '';
  reportCompare(expect, actual, summary + ': 2');
}

expect = a;
actual = a.valueOf();
reportCompare(expect, actual, summary + ': 3');

try
{
  expect = 'function a() {\n    return 1..toSource();\n}';
  actual = "" + a;
  compareSource(expect, actual, summary + ': 4');
}
catch(ex)
{
  actual = ex + '';
  reportCompare(expect, actual, summary + ': 4');
}
