/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var summary = "YieldExpression is and contains an AssignmentExpression";
var actual, expect;

printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

function* gen()
{
  yield (yield (yield 7));
}

var it = gen();

try
{
  if (it.next().value != 7)
    throw "7 not yielded";
  if (it.next(17).value != 17)
    throw "passed-in 17 not yielded";
  if (it.next(undefined).value !== undefined)
    throw "should be able to yield undefined";

  it.next();
}
catch (e)
{
  failed = e;
}

expect = false;
actual = failed;

reportCompare(expect, actual, summary);
