/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER     = "(none)";
var summary = "Sequential yields";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

function fib()
{
  yield 0; // 0
  yield 1; // 1
  yield 1; // 2
  yield 2; // 3
  yield 3; // 4
  yield 5; // 5
  yield 8; // 6
}

var failed = false;
var it = fib();

try
{
  if (it.next() != 0)
    throw "0 failed";
  if (it.next() != 1)
    throw "1 failed";
  if (it.next() != 1)
    throw "2 failed";
  if (it.next() != 2)
    throw "3 failed";
  if (it.next() != 3)
    throw "4 failed";
  if (it.next() != 5)
    throw "5 failed";
  if (it.next() != 8)
    throw "6 failed";

  var stopPassed = false;
  try
  {
    it.next();
  }
  catch (e)
  {
    if (e === StopIteration)
      stopPassed = true;
  }
  if (!stopPassed)
    throw "missing or incorrect StopIteration";
}
catch (e)
{
  failed = e;
}



expect = false;
actual = failed;

reportCompare(expect, actual, summary);
