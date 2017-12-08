/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
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
  printBugNumber(BUGNUMBER);
  printStatus (summary);

// ------- Comment #135 From Gary Kwong [:nth10sd]

// -j is not required:
// ===

  for (let i = 0; i < 9; ++i) {
    let m = i;
    if (i % 3 == 1) {
      print('' + (function() { return m; })());
    }
  }

// Debug & opt crash [@ BindNameToSlot]

// -j is required:
// ===
    for (let a=0;a<3;++a) for (let b=0;b<3;++b) if ((g=a|(a%b))) with({}){}

// Assertion failure: OBJ_IS_CLONED_BLOCK(obj), at ../jsobj.cpp:2392

  reportCompare(expect, actual, summary);
}
