// |reftest| skip-if(Android)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 452498;
var summary = 'TM: upvar2 regression tests';
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

// ------- Comment #99 From Gary Kwong [:nth10sd]

  if (typeof timeout == 'function')
  {
    expectExitCode(6);
    timeout(3);

    while( getter = function() { return y } for (y in y) )( /x/g );
  }

// Assertion failure: lexdep->frameLevel() <= funbox->level, at ../jsparse.cpp:1771
// Crash [@ JSCompiler::setFunctionKinds] near null in opt, -j not required.

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
