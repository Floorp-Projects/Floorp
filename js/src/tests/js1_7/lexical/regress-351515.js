/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 351515;
var summary = 'Invalid uses of yield, let keywords in js17';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

try
{
  expect = 'SyntaxError: yield not in function';
  eval('yield = 1;');
  actual = 'No Error';
}
catch(ex)
{
  actual = ex + '';
}
reportCompare(expect, actual, summary + ': global: yield = 1');

try
{
  expect = 'SyntaxError: syntax error';
  eval('(function(){yield = 1;})');
  actual = 'No Error';
}
catch(ex)
{
  actual = ex + '';
}
reportCompare(expect, actual, summary + ': local: yield = 1');

try
{
  expect = 'SyntaxError: missing variable name';
  eval('let = 1;');
  actual = 'No Error';
}
catch(ex)
{
  actual = ex + '';
}
reportCompare(expect, actual, summary + ': global: let = 1');

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  try
  {
    expect = 'SyntaxError: missing formal parameter';
    eval('function f(yield, let) { return yield+let; }');
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary +
		': function f(yield, let) { return yield+let; }');

  try
  {
    expect = 'SyntaxError: missing variable name';
    eval('var yield = 1;');
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': function () {var yield;}');

  try
  {
    expect = 'SyntaxError: missing variable name';
    eval('var let = 1;');
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': function () { var let;}');

  exitFunc ('test');
}
