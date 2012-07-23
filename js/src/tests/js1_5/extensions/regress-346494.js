/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 346494;
var summary = 'try-catch-finally scope';
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
 
  function g()
  {
    try
    {
      throw "foo";
    }
    catch(e if e == "bar")
    {
    }
    catch(e if e == "baz")
    {
    }
    finally
    {
    }
  }

  expect = "foo";
  try
  {
    g();
    actual = 'No Exception';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary);

  function h()
  {
    try
    {
      throw "foo";
    }
    catch(e if e == "bar")
    {
    }
    catch(e)
    {
    }
    finally
    {
    }
  }

  expect = "No Exception";
  try
  {
    h();
    actual = 'No Exception';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
