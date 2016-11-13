/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 683738;
var summary = 'return with argument and lazy generator detection';
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

  expect = "can't use 'yield' in a function that can return a value";
  try
  {
    actual = 'No Error';
    eval("function foo(x) { if (x) { return this; } else { yield 3; } }");
  }
  catch(ex)
  {
    actual = ex.message;
  }
  reportCompare(expect, actual, summary + ": 1");

  expect = "generator function can't return a value";
  try
  {
    actual = 'No Error';
    eval("function foo(x) { if (x) { yield 3; } else { return this; } }");
  }
  catch(ex)
  {
    actual = ex.message;
  }
  reportCompare(expect, actual, summary + ": 2");

  expect = "can't use 'yield' in a function that can return a value";
  try
  {
    actual = 'No Error';
    eval("function foo(x) { if (x) { return this; } else { (yield 3); } }");
  }
  catch(ex)
  {
    actual = ex.message;
  }
  reportCompare(expect, actual, summary + ": 3");

  expect = "generator function can't return a value";
  try
  {
    actual = 'No Error';
    eval("function foo(x) { if (x) { (yield 3); } else { return this; } }");
  }
  catch(ex)
  {
    actual = ex.message;
  }
  reportCompare(expect, actual, summary + ": 4");

}
