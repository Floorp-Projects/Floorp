/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 466206;
var summary = 'Do not crash due to unrooted function variables';
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

  var f;

  function g() {
    var x = {};
    f = function () { x.y; };
    if (0) yield;
  }

  try { g().next(); } catch (e) {}
  gc();
  f();
 
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
