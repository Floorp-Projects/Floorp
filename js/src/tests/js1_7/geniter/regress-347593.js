/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 347593;
var summary = 'For-each loop with destructuring assignment';
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
 
  expect = '23';
  actual = '';
  for (let [, { a: b }] in [{ a: 2 }, { a: 3 }])
  {
    actual += b;
  }
  reportCompare(expect, actual, summary);

  actual = '';
  for each (let { a: b } in [{ a: 2 }, { a: 3 }])
  {
    actual += b;
  }
  reportCompare(expect, actual, summary);

  expect = 'TypeError';
  actual = '';
  try
  {
    for each (let [, { a: b }] in [{ a: 2 }, { a: 3 }])
    {
      actual += b;
    }
  }
  catch(ex)
  {
    actual = ex.name;
  }
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
