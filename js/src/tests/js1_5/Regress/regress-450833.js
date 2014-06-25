/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 450833;
var summary = 'TM: Multiple trees per entry point';
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
 
  expect = 100;

  jit(true);

  function f(i) {
    for (var m = 0; m < 20; ++m)
      for (var n = 0; n < 100; n += i)
        ;
    return n;
  }

  print(actual = f(1));

  jit(false);

  reportCompare(expect, actual, summary);

  jit(true);

  print(actual = f(.5));

  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
