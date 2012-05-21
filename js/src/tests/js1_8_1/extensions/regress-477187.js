// |reftest| skip-if(Android)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 477187;
var summary = 'timeout script';
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

  if (typeof window != 'undefined' || typeof timeout != 'function')
  {
    print(expect = actual = 'Test skipped due to lack of timeout function');
    reportCompare(expect, actual, summary);
  }
  else
  {
    expectExitCode(6);
    timeout(0.01);
    // Call reportCompare early here to get a result.  The test will fail if
    // the timeout doesn't work and the test framework is forced to terminate
    // the test.
    reportCompare(expect, actual, summary);

    while(1);
  }

  exitFunc ('test');
}
