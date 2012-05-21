/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 346904;
var summary = 'uneval expressions with double negation, negation decrement';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var f;

  // - --
  expect =
    'function () {\n' +
    '    return - --x;\n' +
    '}';
  try
  {
    f = eval('(function () { return - --x; })');
    actual = f + '';
    compareSource(expect, actual, summary);
  }
  catch(ex)
  {
    actual = ex + '';
    reportCompare(expect, actual, summary);
  }

  // - -
  expect =
    'function () {\n' +
    '    return - - x;\n' +
    '}';
  try
  {
    f = eval('(function () { return - - x; })');
    actual = f + '';
    compareSource(expect, actual, summary);
  }
  catch(ex)
  {
    actual = ex + '';
    reportCompare(expect, actual, summary);
  }

  // ---
  expect = 'SyntaxError';
  try
  {
    f = eval('(function () { return ---x; })');
    actual = f + '';
    compareSource(expect, actual, summary);
  }
  catch(ex)
  {
    actual = ex.name;
    reportCompare(expect, actual, summary);
  }

  // + ++
  expect =
    'function () {\n' +
    '    return + ++x;\n' +
    '}';
  try
  {
    f = eval('(function () { return + ++x; })');
    actual = f + '';
    compareSource(expect, actual, summary);
  }
  catch(ex)
  {
    actual = ex + '';
    reportCompare(expect, actual, summary);
  }

  // + +
  expect =
    'function () {\n' +
    '    return + + x;\n' +
    '}';
  try
  {
    f = eval('(function () { return + + x; })');
    actual = f + '';
    compareSource(expect, actual, summary);
  }
  catch(ex)
  {
    actual = ex + '';
    reportCompare(expect, actual, summary);
  }

  // +++

  expect = 'SyntaxError';
  try
  {
    f = eval('(function () { return +++x; })');
    actual = f + '';
  }
  catch(ex)
  {
    actual = ex.name;
  }
  reportCompare(expect, actual, summary);


  exitFunc ('test');
}
