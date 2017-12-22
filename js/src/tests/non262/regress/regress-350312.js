/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var counter = 0;

  function f(x,y) {

    try
    {
      throw x;
    }
    catch(e)
    {
      if (y)
        throw e;
    }
    finally
    {
      try
      {
        actual += 'finally,';
        throw 42;
      }
      catch(e2)
      {
        actual += e2;
        if (++counter > 10)
        {
          throw 'Infinite loop...';
        }
      }
    }
    return 'returned';
  }

  expect = 'finally,42';
  actual = '';

  try
  {
    print('test 1');
    f(2, 1);
  }
  catch(ex)
  {
  }
  reportCompare(expect, actual, summary);

  actual = '';
  try
  {
    print('test 2');
    f(2, 0);
  }
  catch(ex)
  {
  }
  reportCompare(expect, actual, summary);
}
