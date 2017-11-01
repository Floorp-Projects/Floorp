/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 355052;
var summary = 'Do not crash with valueOf:gc and __iterator__';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  expect = /TypeError: .+ is not a function/;
  actual = 'No Error';
  try
  {
    ( {valueOf: gc} - [].a )();
  }
  catch(ex)
  {
    actual = ex + '';
  }

  reportMatch(expect, actual, summary);
}
