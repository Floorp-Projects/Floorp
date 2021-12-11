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

// ------- Comment #6 From Andreas Gal :gal

  function foo() {
    var x = 4;
    var f = (function() { return x++; });
    var g = (function() { return x++; });
    return [f,g];
  }

  var bar = foo();

  expect = '9';
  actual = 0;

  bar[0]();
  bar[1]();

  actual = String(expect);

  reportCompare(expect, actual, summary);
}
