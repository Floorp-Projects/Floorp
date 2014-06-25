/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465484;
var summary = 'TM: Do not assert: _allocator.active[FST0] && _fpuStkDepth == -1 || ' +
  '!_allocator.active[FST0] && _fpuStkDepth == 0';
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
 
  jit(true);
  for each (let a in [2, 2, 2]) { a %= a; a %= a; }
  jit(false);
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
