/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER     = "(none)";
var summary = "Iterator() test";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

function Array_equals(a, b)
{
  if (!(a instanceof Array) || !(b instanceof Array))
    throw new Error("Arguments not both of type Array");
  if (a.length != b.length)
    return false;
  for (var i = 0, sz = a.length; i < sz; i++)
    if (a[i] !== b[i])
      return false;
  return true;
}

var meow = "meow", oink = "oink", baa = "baa";

var it = Iterator([meow, oink, baa]);
var it2 = Iterator([meow, oink, baa], true);

try
{
  if (!Array_equals(it.next(), [0, meow]))
    throw [0, meow];
  if (!Array_equals(it.next(), [1, oink]))
    throw [1, oink];
  if (!Array_equals(it.next(), [2, baa]))
    throw [2, baa];

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

  if (it2.next() != 0)
    throw "wanted key=0";
  if (it2.next() != 1)
    throw "wanted key=1";
  if (it2.next() != 2)
    throw "wanted key=2";

  var stopPassed = false;
  try
  {
    it2.next();
  }
  catch (e)
  {
    if (e === StopIteration)
      stopPassed = true;
  }

  if (!stopPassed)
    throw "it2: missing or incorrect StopIteration";
}
catch (e)
{
  failed = e;
}

expect = false;
actual = failed;

reportCompare(expect, actual, summary);
