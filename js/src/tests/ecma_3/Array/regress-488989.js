/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 488989;
var summary = 'Array.prototype.push for non-arrays near max-array-index limit';
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

  var stack = { push: [].push }; stack.length = Math.pow(2, 37);
  stack.push(-2, -1, 0);

  var stack = { push: [].push }; stack.length = Math.pow(2, 5);
  stack.push(-2, -1, 0);

  var stack = { push: [].push }; stack.length = Math.pow(2, 32) -2;
  stack.push(-2, -1, 0);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
