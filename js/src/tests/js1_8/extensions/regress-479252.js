// |reftest| skip
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 479252;
var summary = 'Avoid watchdog ticks when idle in shell';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  if (typeof sleep != 'function' || typeof scatter != 'function' ||
      typeof timeout != 'function')
  {
    print(expect = actual = 'Test skipped: requires mulithreads and timeout.');
  }
  else
  {
    expectExitCode(6);

    function f() { sleep(100); }
    timeout(1.0);
    scatter([f,f,f,f,f]);
  }

  reportCompare(expect, actual, summary);
}
