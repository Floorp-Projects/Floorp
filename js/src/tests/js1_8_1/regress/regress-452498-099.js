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

  try
  {
    eval("(function x(){x.(this)} )();");
  }
  catch(ex)
  {
  }

// Assertion failure: (uint32_t)(index_) < atoms_->length, at ../jsinterp.cpp:327
// Crash [@ js_FullTestPropertyCache] at null in opt, -j not required.

// =====

  try
  {
    (function(){try {x} finally {}; ([x in []] for each (x in x))})();
  }
  catch(ex)
  {
  }

// Assertion failure: lexdep->frameLevel() <= funbox->level, at ../jsparse.cpp:1735
// Crash [@ BindNameToSlot] near null in opt, -j not required.

// =====

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
