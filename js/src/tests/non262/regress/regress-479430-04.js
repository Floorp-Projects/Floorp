// |reftest| skip-if(Android)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 479430;
var summary = 'Missing operation callback checks';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  if (typeof timeout== 'function')
  {
    expectExitCode(6);

    timeout(0.01);

    function f(n)
    {
      if (n != 0) {
        try { f(n - 1); } catch (e) {}
        try { f(n - 1); } catch (e) {}
      }
      name_that_does_not_exists;
    }

    f(100);
  }

  reportCompare(expect, actual, summary);
}
