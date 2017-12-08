/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 471660;
var summary = 'TM: Do not assert: !(fp->flags & JSFRAME_POP_BLOCKS)';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);


  y = {"a":1};

  for (var w = 0; w < 5; ++w) {

    { let y; do break ; while (true); }
    for (let x of [{}, function(){}]) {y}

  }


  reportCompare(expect, actual, summary);
}
