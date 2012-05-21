/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER     = "(none)";
var summary = "calling it.close multiple times is harmless";
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

  // closing an already-closed generator is a no-op
  it.close();
  it.close();
  it.close();

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
    throw "a closed iterator throws StopIteration on next";
}
catch (e)
{
  failed = e;
}



expect = false;
actual = failed;

reportCompare(expect, actual, summary);
