/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 351120;
var summary = 'Incorrect error messages with yield expressions';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  expect = /TypeError:.*(is not a function|Cannot find function).*/;
  actual = '';
  try
  {
    (function*() { yield [].z({}); })().next();
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportMatch(expect, actual, summary);
}
