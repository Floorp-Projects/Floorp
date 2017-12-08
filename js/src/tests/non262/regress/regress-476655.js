/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 476655;
var summary = 'Do not assert: depth <= (size_t) (fp->regs->sp - StackBase(fp))';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);


  try
  {
    eval(
      "for(let y in ['', '']) try {for(let y in ['', '']) ( /x/g ); } finally {" +
      "with({}){} } this.zzz.zzz"

      );
  }
  catch(ex)
  {
  }

  reportCompare(expect, actual, summary);
}
