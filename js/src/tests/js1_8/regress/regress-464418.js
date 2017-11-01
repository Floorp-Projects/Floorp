// |reftest| skip-if(Android)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 464418;
var summary = 'Do not assert: fp->slots + fp->script->nfixed + ' +
  'js_ReconstructStackDepth(cx, fp->script, fp->regs->pc) == fp->regs->sp';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 

  if (typeof gczeal == 'function')
  {
    gczeal(2);
  }

  for (let q = 0; q < 50; ++q) {
    new Function("for (var i = 0; i < 5; ++i) { } ")();
    var w = "r".match(/r/);
    new Function("for (var j = 0; j < 1; ++j) { } ")();
  }


  if (typeof gczeal == 'function')
  {
    gczeal(0);
  }

  reportCompare(expect, actual, summary);
}
