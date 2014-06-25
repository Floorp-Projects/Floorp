/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465137;
var summary = '!NaN is not false';
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
 
  expect = 'falsy,falsy,falsy,falsy,falsy,';
  actual = '';

  jit(true);

  for (var i=0;i<5;++i) actual += (!(NaN) ? "falsy" : "truthy") + ',';

  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
