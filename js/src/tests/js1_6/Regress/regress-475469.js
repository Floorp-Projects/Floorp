/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 475469;
var summary = 'TM: Do not crash @ FramePCOffset';
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

//  print('Note this test originally required jit enabled with the environment ');
//  print('variable TRACEMONKEY=verbose defined. Note that the calls to enable ');
//  print('jit are necessary for the crash.');

  jit(true);
  [1,2,3].map(function(v, i, t) { return /a/gi.exec(v); });
  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}

