/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 466787;
var summary = 'TM: new Number() should stay a number during recording';
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

  expect = '4444';

  jit(true);

  for (let j = 0; j < 4; ++j) { for each (let one in [new Number(1)]) {
        print(actual += '' + (3 + one)); } }

  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
