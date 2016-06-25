/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 461930;
var summary = 'TM: Do not assert: count == _stats.pages';
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


  function gen() { for (let j = 0; j < 4; ++j) { yield 1; yield 2; gc(); } }
  for (let i in gen()) { }


  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
