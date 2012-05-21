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

// ------- Comment #130 From Gary Kwong [:nth10sd]

// Does not require -j:
// =====
  ((function x()x in []) for (y in []))

//Assertion failure: lexdep->frameLevel() <= funbox->level, at ../jsparse.cpp:1778
// Opt crash [@ JSCompiler::setFunctionKinds]
// =====
    let(x=[]) function(){try {x} catch(x) {} }

// Assertion failure: cg->upvars.lookup(atom), at ../jsemit.cpp:2034
// =====
  try
  {
    eval('for(let [y] = (let (x) (y)) in []) function(){}');
  }
  catch(ex)
  {
  }
// Assertion failure: !(pnu->pn_dflags & PND_BOUND), at ../jsemit.cpp:1818
// =====


// Requires -j:
// =====
  for (var x = 0; x < 3; ++x) { new function(){} }

// Assertion failure: cx->bailExit, at ../jstracer.cpp:4672
// Opt crash [@ LeaveTree] near null

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
