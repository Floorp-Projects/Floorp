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

// ------- Comment #138 From Gary Kwong [:nth10sd]

// Does not require -j:
// ===
  ((function x(){ yield (x = undefined) } ) for (y in /x/));

// Assertion failure: lexdep->frameLevel() <= funbox->level, at ../jsparse.cpp:1820
// ===
  try
  {
    for(let x in ( x for (y in x) for each (x in []) )) y;
  }
  catch(ex)
  {
  }
// Assertion failure: cg->upvars.lookup(atom), at ../jsemit.cpp:2034
// ===

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
