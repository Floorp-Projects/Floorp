/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff
 */

var gTestfile = 'regress-452498-053.js';
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

// ------- Comment #53 From Jason Orendorff

// Assertion failure: (slot) < (uint32)(obj)->dslots[-1]
// at ../jsobj.cpp:5559
// On the last line of BindLet, we have
//    JS_SetReservedSlot(cx, blockObj, index, PRIVATE_TO_JSVAL(pn));
// but this uses reserved slots as though they were unlimited.
// blockObj only has 2.
  let (a=0, b=1, c=2) {}

// In RecycleTree at ../jsparse.cpp:315, we hit
//     JS_NOT_REACHED("RecycleUseDefKids");
// pn->pn_type is TOK_UNARYOP
// pn->pn_op   is JSOP_XMLNAME
// pn->pn_defn is 1
// pn->pn_used is 1
  try
  {
    @foo; 0;
  }
  catch(ex)
  {
  }
// Calls LinkUseToDef with pn->pn_defn == 1.
//
// If you say "var x;" first, then run this case, it gets further,
// crashing in NoteLValue like the first case in comment 52.
//
  try
  {
    for (var [x] = x in y) var x;
  }
  catch(ex)
  {
  }
// Assertion failure: !pn2->pn_defn, at ../jsparse.h:461
// Another case where some optimization is going on.
  try
  {
    if (true && @foo) ;
  }
  catch(ex)
  {
  }
// Assertion failure: scope->object == ctor
// in js_FastNewObject at ../jsbuiltins.cpp:237
//
// With the patch, we're new-ing a different function each time, and the
// .prototype property is missing.
//
  for (var z = 0; z < 3; z++) { (new function(){}); }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
