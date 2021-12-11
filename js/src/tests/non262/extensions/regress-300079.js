/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 300079;
var summary = "precompiled functions should inherit from current window's Function.prototype";
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  if (typeof clone == 'undefined') {
    expect = 'SKIPPED';
    actual = 'SKIPPED';
  }
  else {
    expect = 'PASSED';

    f = evaluate("(function () { return a * a; })", {envChainObject: {a: 3}});
    gc();
    try {
      a_squared = f(2);
      if (a_squared != 9)
        throw "Unexpected return from g: a_squared == " + a_squared;
      actual = "PASSED";
    } catch (e) {
      actual = "FAILED: " + e;
    }
  }
 
  reportCompare(expect, actual, summary);
}
