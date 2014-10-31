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
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

// ------- Comment #117 From Gary Kwong [:nth10sd]

// The following all do not require -j.

// =====

  try
  {
    eval('x; function  x(){}; const x;');
  }
  catch(ex)
  {
  }

// Assertion failure: !pn->isPlaceholder(), at ../jsparse.cpp:4876
// =====
  (function(){ var x; eval("var x; x = null"); })();

// Assertion failure: !(pnu->pn_dflags & PND_BOUND), at ../jsemit.cpp:1818
// =====
  (function(){const x = 0, y = delete x;})()

// Assertion failure: JOF_OPTYPE(op) == JOF_ATOM, at ../jsemit.cpp:1710
// =====
    try
    {
      (function(){(yield []) (function(){with({}){x} }); const x;})();
    }
    catch(ex)
    {
    }

// Assertion failure: cg->upvars.lookup(atom), at ../jsemit.cpp:2022
// =====
  try
  {
    (function(){([]) ((function(q) { return q; })for (each in [1,2]))})();
  }
  catch(ex)
  {
  }
// Assertion failure: lexdep->frameLevel() <= funbox->level, at ../jsparse.cpp:1782
// Opt crash [@ JSCompiler::setFunctionKinds] near null
// =====

  try
  {
    eval("((x1) > [(x)(function() { x;}) for each (x in x)])()");
  }
  catch(ex)
  {
  }

// Assertion failure: pnu->pn_lexdef == dn, at ../jsemit.cpp:1817
// =====
  uneval(function(){for(var [arguments] = ({ get y(){} }) in y ) (x);});

// Assertion failure: n != 0, at ../jsfun.cpp:2689
// =====
  try
  {
    eval('(function(){{for(c in (function (){ for(x in (x1))window} )()) {const x;} }})();');
  }
  catch(ex)
  {
  }

// Assertion failure: op == JSOP_GETLOCAL, at ../jsemit.cpp:4557
// =====
  try
  {
    (eval("(function(){let x , x =  (x for (x in null))});"))();
  }
  catch(ex)
  {
  }

// Assertion failure: (fun->u.i.script)->upvarsOffset != 0, at ../jsfun.cpp:1537
// Opt crash [@ js_NewFlatClosure] near null
// =====
  "" + function(){for(var [x] in x1) ([]); function x(){}}

// Assertion failure: cg->stackDepth == stackDepth, at ../jsemit.cpp:3664
// Opt crash [@ JS_ArenaRealloc] near null
// =====
  try
  {
    eval(
      "for (a in (function(){" +
      "      for each (let x in ['']) { return new function x1 (\u3056) { yield x } ();" +
      "        }})())" +
      "  function(){}"
      );
  }
  catch(ex)
  {
  }
// Crash [@ js_Interpret] near null
// =====

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
