/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 466128;
var summary = 'Do not assert: staticLevel == script->staticLevel, at ../jsobj.cpp';
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

  function f(foo) {
    if (a % 2 == 1) {
      try {
        eval(foo);
      } catch(e) {}
    }
  }
  a = 1;
  f("eval(\"x\")");
  f("x");

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
