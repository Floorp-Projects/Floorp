// |reftest| require-or(debugMode,skip)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 431428;
var summary = 'Do not crash with for..in, trap';
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
 
  function f() { 
    for ( var a in [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17]) { }
  }

  if (typeof trap == 'function' && typeof setDebug == 'function')
  {
    setDebug(true);
    "" + f;
    trap(f, 0, "");
    "" + f;
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
