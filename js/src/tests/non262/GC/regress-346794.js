// |reftest| skip -- slow, killed
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 346794;
var summary = 'Do not crash';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  // either don't crash or run out of memory
  expectExitCode(0);
  expectExitCode(3);

  function boo() {
    s = '';
    for (;;) {
      try {
        new RegExp(s + '[\\');
      } catch(e) {}
      s += 'q';
    }
  }

  boo();

  reportCompare(expect, actual, summary);
}
