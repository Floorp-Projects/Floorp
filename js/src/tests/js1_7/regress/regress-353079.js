/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 353079;
var summary = 'Do not Assert op == JSOP_LEAVEBLOCKEXPR... with WAY_TOO_MUCH_GC';
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
 
  for (let a in [1]) let (x) {
    for (let y in ((function(id2) { return id2; })( '' ))) { } }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
