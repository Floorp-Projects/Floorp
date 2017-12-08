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

// ------- Comment #58 From Gary Kwong [:nth10sd]

  function foo(x) { var x = x }

// Assertion failure: dn->kind() == ((data->op == JSOP_DEFCONST) ? JSDefinition::CONST : JSDefinition::VAR), at ../jsparse.cpp:2595
  reportCompare(expect, actual, summary);
}
