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

// ------- Comment #114 From Gary Kwong [:nth10sd]

  if (typeof timeout == 'function')
  {
    timeout(3);
    try
    {
      eval('while(x|={}) with({}) const x;');
    }
    catch(ex)
    {
    }
    reportCompare(expect, actual, '');
  }

// Assertion failure: cg->stackDepth >= 0, at ../jsemit.cpp:185

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
