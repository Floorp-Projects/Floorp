/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 349507;
var summary = 'Do not assert with let block, let statement and const';
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

  expect = /TypeError: redeclaration of const b/;
  try
  {
    eval('(function() { let(x = 1) { const b = 2 }; let b = 3; })');
  }
  catch(ex)
  {
    actual = ex + '';
  }

  reportMatch(expect, actual, summary);

  exitFunc ('test');
}
