/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 420919;
var summary = 'this.u.v = 1 should report this.u is undefined';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  expect = /TypeError: (this.u is undefined|can't access property "v", this.u is undefined)/;

  try
  {
    this.u.v = 1;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportMatch(expect, actual, summary);
}
