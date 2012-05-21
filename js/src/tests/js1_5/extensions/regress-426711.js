/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 426711;
var summary = 'Setting window.__count__ causes a crash';
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
 
  if (typeof window != 'undefined' && '__count__' in window)
  {
    window.__count__ = 0;
  }
  else
  {
    expect = actual = 'Test skipped. Requires window.__count__';
  }
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
