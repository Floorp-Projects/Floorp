/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 387501;
var summary =
  'Array.prototype.toString|toLocaleString are generic, ' +
  'Array.prototype.toSource is not generic';
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
 
  expect = '[object String]';
  actual = Array.prototype.toString.call((new String('foo')));
  assertEq(actual, expect, summary);

  expect = 'f,o,o';
  actual = Array.prototype.toLocaleString.call((new String('foo')));
  assertEq(actual, expect, summary);

  if (typeof Array.prototype.toSource != 'undefined')
  {
    try
    {
      Array.prototype.toSource.call(new String('foo'));
      throw new Error("didn't throw");
    }
    catch(ex)
    {
      assertEq(ex instanceof TypeError, true,
               "wrong error thrown: expected TypeError, got " + ex);
    }
  }

  reportCompare(true, true, "Tests complete");

  exitFunc ('test');
}
