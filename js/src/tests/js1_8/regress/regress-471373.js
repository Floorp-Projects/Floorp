/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 471373;
var summary = 'TM: do not assert: (size_t)(regs.pc - script->code) < script->length';
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

  if (typeof window == 'undefined')
  {
    expectExitCode(5);
  }

  jit(true);

  function g() {
    var x = {};
    for (var b = 0; b < 2; ++b) {
      yield x;
      for (var c = 0; c < 10;++c) {
        x.r = x;
      }
    }
  }
  for (let y in g()) { }
  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
