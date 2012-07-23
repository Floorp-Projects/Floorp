/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 467495;
var summary = 'TCF_FUN_CLOSURE_VS_VAR is necessary';
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

  function f(x)
  {
    actual = '';
    var g;
    print(actual += typeof g + ',');

    if (x)
      function g(){};

    print(actual += g);
  }

  expect = 'undefined,undefined';
  f(0);

  reportCompare(expect, actual, summary + ': f(0): ');

  expect = 'undefined,function g(){}';

  f(1);

  reportCompare(expect, actual, summary + ': f(1): ');

  exitFunc ('test');
}
