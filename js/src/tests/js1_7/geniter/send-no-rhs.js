/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER     = "(none)";
var summary = "|it.send(o)| without an RHS";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

function gen()
{
  yield 7;
  yield 3;
}

var it = gen();

try
{
  if (it.next() != 7)
    throw "7 not yielded";
  if (it.send(12) != 3)
    throw "3 not yielded";

  var stopPassed = false;
  try
  {
    it.send(35); // resultant value of |yield 3;|
  }
  catch (e)
  {
    if (e === StopIteration)
      stopPassed = true;
  }

  if (!stopPassed)
    throw "it: missing or incorrect StopIteration";
}
catch (e)
{
  failed = e;
}

expect = false;
actual = failed;

reportCompare(expect, actual, summary);
