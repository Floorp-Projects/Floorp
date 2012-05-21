/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 472528;
var summary = 'Do not assert: !fp->blockChain';
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
    for (let i = 0; i < 4; ++i) {
      for (let j = 0; j < 2; ++j) { }
      let (x) (function(){});
    }
  }
  catch(ex)
  {
  }
  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
