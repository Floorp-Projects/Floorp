/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 453397;
var summary = 'Do not assert with JIT: script->main <= target && target < script->code + script->length';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  jit(true);

  function computeEscapeSpeed(real) {
    for (var j = 1; j < 4; ++j) {
      if (real > 2) {
      }
    }
  }

  const numRows = 4;
  const numCols = 4;
  var realStep = 1.5;
  for (var i = 0, curReal = -2.1;
       i < numCols;
       ++i, curReal += realStep) {
    for (var j = 0; j < numRows; ++j) {
      computeEscapeSpeed(curReal);
    }
  }

  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
