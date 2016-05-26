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

// ------- Comment #92 From Jesse Ruderman

  expect = 'SyntaxError: redeclaration of formal parameter e';
  try
  {
    eval('(function (e) { var e; const e = undefined; });');
  }
  catch(ex)
  {
    actual = ex + '';
  }
// Without patch: "SyntaxError: redeclaration of var e"
// expected new behavior // With patch:    "SyntaxError: redeclaration of formal parameter e:"

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
