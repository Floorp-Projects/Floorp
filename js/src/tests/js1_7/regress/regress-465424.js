/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465424;
var summary = 'TM: issue with post-decrement operator';
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

  expect = '0,1,2,3,4,';

  jit(true);
  for (let j=0;j<5;++j) { jj=j; print(actual += '' + (jj--) + ',') }
  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
