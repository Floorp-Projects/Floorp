/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER     = "364603";
var summary = "The value placed in a filtered array for an element is the " +
  " element's value before the callback is run, not after";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

function mutate(val, index, arr)
{
  arr[index] = "mutated";
  return true;
}

function assertEqual(v1, v2, msg)
{
  if (v1 !== v2)
    throw msg;
}

try
{
  var a = [1, 2];
  var m = a.filter(mutate);

  assertEqual(a[0], "mutated", "Array a not mutated!");
  assertEqual(a[1], "mutated", "Array a not mutated!");

  assertEqual(m[0], 1, "Filtered value is value before callback is run");
  assertEqual(m[1], 2, "Filtered value is value before callback is run");
}
catch (e)
{
  failed = e;
}


expect = false;
actual = failed;

reportCompare(expect, actual, summary);
