/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 352197;
var summary = 'TypeError if yield after return value in a block';
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

  expect = /TypeError: anonymous generator function returns a value/;
  try
  {
    var gen = eval('(function() { { return 5; } yield 3; })');
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportMatch(expect, actual, summary);

  exitFunc ('test');
}
