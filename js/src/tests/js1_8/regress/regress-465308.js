/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465308;
var summary = '((0x60000009) * 0x60000009))';
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
 
  expect = '-1073741824,-1073741824,-1073741824,-1073741824,-1073741824,';

  jit(true);
  for (let j=0;j<5;++j) 
    print(actual += "" + (0 | ((0x60000009) * 0x60000009)) + ',');
  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
