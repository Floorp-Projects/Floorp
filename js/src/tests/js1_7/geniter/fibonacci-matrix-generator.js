/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER     = "(none)";
var summary = "Fibonacci generator by matrix multiplication";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

function fib()
{
  var init = [1, 0];
  var mx = [[1, 1], [1, 0]];
  while (true)
  {
    yield init[1];
    var tmp = [,];
    tmp[0] =
      mx[0][0]*init[0] + mx[0][1]*init[1];
    tmp[1] =
      mx[1][0]*init[0] + mx[1][1]*init[1];
    init = tmp;
  }
}

var failed = false;
var it = fib();

try
{
  if (it.next() != 0)
    throw "F_0 failed";
  if (it.next() != 1)
    throw "F_1 failed";
  if (it.next() != 1)
    throw "F_2 failed";
  if (it.next() != 2)
    throw "F_3 failed";
  if (it.next() != 3)
    throw "F_4 failed";
  if (it.next() != 5)
    throw "F_5 failed";
  if (it.next() != 8)
    throw "F_6 failed";
}
catch (e)
{
  failed = e;
}



expect = false;
actual = failed;

reportCompare(expect, actual, summary);
