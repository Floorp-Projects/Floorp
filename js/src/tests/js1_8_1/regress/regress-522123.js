/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 522123;
var summary = 'Indirect eval confuses scope chain';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
var x=1;

test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  expect = 1;

  evil=eval;
  {
    let x=2;
    actual = evil("x");
  };

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}

reportCompare(true, true);
