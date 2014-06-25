/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jason Orendorff
 */

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

// ------- Comment #52 From Jason Orendorff

// Crash in NoteLValue, called from BindDestructuringVar.
// NoteLValue assumes pn->pn_lexdef is non-null, but here
// pn is itself the definition of x.
  for (var [x]=0 in null) ;

// This one only crashes when executed from a file.
// Assertion failure: pn != dn->dn_uses, at ../jsparse.cpp:1131
  for (var f in null)
    ;
  var f = 1;
  (f)

// Assertion failure: pnu->pn_cookie == FREE_UPVAR_COOKIE, at ../jsemit.cpp:1815
// In EmitEnterBlock. x has one use, which is pnu here.
// pnu is indeed a name, but pnu->pn_cookie is 0.
    try { eval('let (x = 1) { var x; }'); } catch(ex) {}

// Assertion failure: cg->upvars.lookup(atom), at ../jsemit.cpp:1992
// atom="x", upvars is empty.
  (1 for each (x in x));

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
