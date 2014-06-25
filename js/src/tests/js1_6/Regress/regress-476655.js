/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 476655;
var summary = 'TM: Do not assert: count <= (size_t) (fp->regs->sp - StackBase(fp) - depth)';
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

  try
  {
    eval(
      "(function (){ for (var y in this) {} })();" +
      "[''.watch(\"\", function(){}) for each (x in ['', '', eval, '', '']) if " +
      "(x)].map(Function)"
      );
  }
  catch(ex)
  {
  }

  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
