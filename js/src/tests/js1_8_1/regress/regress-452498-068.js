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

// =====

  foo = "" + new Function("while(\u3056){let \u3056 = x}");

// =====

  function a(){ let c; eval("let c, y"); }
  a();

// =====

  try
  {
    {x: 1e+81 ? c : arguments}
  }
  catch(ex)
  {
  }

// =====

  (function(q){return q;} for each (\u3056 in []))

// =====

  for(
    const NaN;
    this.__defineSetter__("x4", function(){});
    (eval("", (p={})))) let ({} = (((x ))(function ([]) {})), x1) y;

// =====

  function f(){ var c; eval("{var c = NaN, c;}"); }
  f();

// =====
  try
  {
    eval(
      '  x\n' +
      '    let(x) {\n' +
      '    var x\n'
      );
  }
  catch(ex)
  {
  }

// =====

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
