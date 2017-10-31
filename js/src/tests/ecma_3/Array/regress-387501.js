/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 387501;
var summary =
  'Array.prototype.toString|toLocaleString|toSource are generic';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  expect = '[object String]';
  actual = Array.prototype.toString.call((new String('foo')));
  assertEq(actual, expect, summary);

  expect = 'f,o,o';
  actual = Array.prototype.toLocaleString.call((new String('foo')));
  assertEq(actual, expect, summary);

  assertEq('["f", "o", "o"]', Array.prototype.toSource.call(new String('foo')));

  if (typeof Array.prototype.toSource != 'undefined')
  {
    try
    {
      Array.prototype.toSource.call('foo');
      throw new Error("didn't throw");
    }
    catch(ex)
    {
      assertEq(ex instanceof TypeError, true,
               "wrong error thrown: expected TypeError, got " + ex);
    }
  }

  reportCompare(true, true, "Tests complete");
}
