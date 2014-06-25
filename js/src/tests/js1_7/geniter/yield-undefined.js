/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
// Note that this syntax isn't in the most recently posted ES4 TG1 wiki export,
// either in the specification parts or in the grammar, so this test might be
// Spidermonkey-specific.
var BUGNUMBER     = "(none)";
var summary = "|yield;| is equivalent to |yield undefined;| ";
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
  yield;
  yield 3;
}

var it = gen();

try
{
  if (it.next() != 7)
    throw "7 not yielded";
  if (it.next() !== undefined)
    throw "|yield;| should be equivalent to |yield undefined;|";
  if (it.next() != 3)
    throw "3 not yielded";

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
    throw "it: missing or incorrect StopIteration";
}
catch (e)
{
  failed = e;
}

expect = false;
actual = failed;

reportCompare(expect, actual, summary);
