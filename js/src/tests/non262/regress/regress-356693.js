/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 356693;
var summary = 'Do not assert: pn2->pn_op == JSOP_SETCALL';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  expect = 'ReferenceError: x is not defined';
  try
  {
    delete (0 ? 3 : x());
  }
  catch(ex)
  {
    actual = ex + '';
  }

  reportCompare(expect, actual, summary);
}
