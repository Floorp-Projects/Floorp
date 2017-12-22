/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 477733;
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


  function g() {
    [];
  }

  try {
    d.d.d;
  } catch(e) {
    void (function(){});
  }

  for (var o in [1, 2, 3]) {
    g();
  }


  reportCompare(expect, actual, summary);
}
