/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 470176;
var summary = 'let-fun should not be able to modify constants';
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
 
  const e = 8; 

  expect = e;

  jit (true);

  let (f = function() { for (var h=0;h<6;++h) ++e; }) { f(); }

  actual = e;

  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
