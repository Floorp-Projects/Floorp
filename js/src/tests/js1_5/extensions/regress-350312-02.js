/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 350312;
var summary = 'Accessing wrong stack slot with nested catch/finally';
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

  function createPrint(obj)
  {
    return new Function("actual += " + obj + " + ','; " +
			"print(" + obj + ");");
  }

  function createThrow(obj)
  {
    return new Function("throw " + obj + "; ");
  }


  function f(a, b, c)
  {
    try {
      a();
    } catch (e if e == null) {
      b();
    } finally {
      c();
    }
  }

  print('test 1');
  expect = 'a,c,';
  actual = '';
  try
  {
    f(createPrint("'a'"), createPrint("'b'"), createPrint("'c'"));
  }
  catch(ex)
  {
    actual += 'caught ' + ex;
  }
  reportCompare(expect, actual, summary + ': 1');

  print('test 2');
  expect = 'c,caught a';
  actual = '';
  try
  {
    f(createThrow("'a'"), createPrint("'b'"), createPrint("'c'"));
  }
  catch(ex)
  {
    actual += 'caught ' + ex;
  }
  reportCompare(expect, actual, summary + ': 2');

  print('test 3');
  expect = 'b,c,';
  actual = '';
  try
  {
    f(createThrow("null"), createPrint("'b'"), createPrint("'c'"));
  }
  catch(ex)
  {
    actual += 'caught ' + ex;
  }
  reportCompare(expect, actual, summary + ': 3');

  print('test 4');
  expect = 'a,c,';
  actual = '';
  try
  {
    f(createPrint("'a'"), createThrow("'b'"), createPrint("'c'"));
  }
  catch(ex)
  {
    actual += 'caught ' + ex;
  }
  reportCompare(expect, actual, summary + ': 4');

  print('test 5');
  expect = 'c,caught b';
  actual = '';
  try
  {
    f(createThrow("null"), createThrow("'b'"), createPrint("'c'"));
  }
  catch(ex)
  {
    actual += 'caught ' + ex;
  }
  reportCompare(expect, actual, summary + ': 5');

  exitFunc ('test');
}
