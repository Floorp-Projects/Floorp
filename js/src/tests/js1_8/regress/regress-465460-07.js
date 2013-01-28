/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465460;
var summary = 'TM: valueOf in a loop: do not assert';
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
 
  expect = actual = 'pass';

  jit (true);

  try
  {
    e = {}; for (j=0;j<3;++j) { 3 | e; } "PASS";
  }
  catch(ex)
  {
    actual = ex + '';
  }

  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
