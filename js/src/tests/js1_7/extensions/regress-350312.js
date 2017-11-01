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

  var iter;
  function* gen()
  {
    try {
      yield iter;
    } catch (e if e == null) {
      actual += 'CATCH,';
      print("CATCH");
    } finally {
      actual += 'FINALLY';
      print("FINALLY");
    }
  }

  expect = 'FINALLY';
  actual = '';
  (iter = gen()).next().value.return();
  reportCompare(expect, actual, summary);

  expect = 'FINALLY';
  actual = '';
  try
  {
    (iter = gen()).next().value.throw(1);
  }
  catch(ex)
  {
  }
  reportCompare(expect, actual, summary);

  expect = 'CATCH,FINALLY';
  actual = '';
  try
  {
    (iter = gen()).next().value.throw(null);
  }
  catch(ex)
  {
  }
  reportCompare(expect, actual, summary);

  reportCompare((iter = gen()).next().value.next().value, undefined, summary);
}
