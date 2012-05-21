// |reftest| require-or(debugMode,skip)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 429248;
var summary = 'Do not assert: 0';
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
 
  function c() { do{}while(0) }

  if (typeof trap == 'function' && typeof setDebug == 'function')
  {
    setDebug(true);
    trap(c, 0, "");
  }
  c + '';

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
