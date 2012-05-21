// |reftest| skip-if(Android)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 452168;
var summary = 'Do not crash with gczeal 2, JIT: @ avmplus::List or @ nanojit::LirBuffer::validate';
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

  if (typeof gczeal == 'undefined')
  {
      expect = actual = 'Test requires gczeal, skipped.';
  }
  else
  {
    jit(true);
    gczeal(2);

    var a, b; gczeal(2); (function() { for (var p in this) { } })();

    gczeal(0);
    jit(false);
  }
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
