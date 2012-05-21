// |reftest| require-or(debugMode,skip)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 422137;
var summary = 'Do not assert or bogo OOM with debugger trap on JOF_CALL bytecode';
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
 
  function f() { return a(); }

  if (typeof trap == 'function' && typeof setDebug == 'function')
  {
    setDebug(true);
    trap(f, 0, "print('trap')");
  }
  f + '';

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
