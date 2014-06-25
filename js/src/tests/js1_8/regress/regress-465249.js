/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465249;
var summary = 'Do not assert: (m != JSVAL_INT) || isInt32(*vp)';
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

  eval("for (let j = 0; j < 5; ++j) { (0x50505050) + (0x50505050); }");

  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
