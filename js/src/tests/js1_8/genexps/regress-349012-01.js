/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 349012;
var summary = 'closing a generator fails to report error if yield during close is ignored';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

if (typeof quit != 'undefined')
{
  quit(0);
}

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  expect = "Inner finally,Outer finally";

  function gen()
  {
    try {
      try {
        yield 1;
      } finally {
        actual += "Inner finally";
        yield 2;
      }
    } finally {
      actual += ",Outer finally";
    }
  }

  try {
    for (var i in gen())
      break;
  } catch (e) {
    if (!(e instanceof TypeError))
      throw e;
  }

  reportCompare(expect, actual, summary);
  exitFunc ('test');
}
