/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 470300;
var summary = 'TM: Do not assert: !fp->blockChain || OBJ_GET_PARENT(cx, obj) == fp->blockChain';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 

  for (let a = 0; a < 7; ++a) { let e = 8; if (a > 3) { let x; } }


  reportCompare(expect, actual, summary);
}
