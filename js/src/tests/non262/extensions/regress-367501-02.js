/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 367501;
var summary = 'getter/setter crashes';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  try
  {
    expect = 'undefined';
    var a = { set x(v) {} };
    for (var i = 0; i < 92169 - 3; ++i) a[i] = 1;
    actual = a.x + '';
    actual = a.x + '';
  }
  catch(ex)
  {
  }
  reportCompare(expect, actual, summary);
}
