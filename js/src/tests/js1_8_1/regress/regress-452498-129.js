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

// ------- Comment #129 From Gary Kwong [:nth10sd]

// Does not require -j:
// =====
try {
    eval("({ set x x () { for(x in function(){}){}}  })");
} catch (e)
{
}

// Assertion failure: JOF_OPTYPE(op) == JOF_ATOM, at ../jsemit.cpp:1710
// =====

try
{
    (function (){ eval("(function(){delete !function(){}});"); })();
}
catch(ex)
{
}
// Debug crash [@ JSParseNode::isFunArg] at null
// Opt crash [@ FindFunArgs] near null
// =====

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
