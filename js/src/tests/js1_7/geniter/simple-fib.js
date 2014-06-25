/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER     = 326466;  // bug 326466, comment 1
var summary = "Simple Fibonacci iterator";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

function fib()
{
  var a = 0, b = 1;
  while (true)
  {
    yield a;
    var t = a;
    a = b;
    b += t;
  }
}

var failed = false;

try
{
  var g = fib();

  if (g.next() != 0)
    throw "F_0 = 0";
  if (g.next() != 1)
    throw "F_1 = 1";
  if (g.next() != 1)
    throw "F_2 = 1";
  if (g.next() != 2)
    throw "F_3 = 2";
}
catch (e)
{
  failed = e;
}



expect = false;
actual = failed;

reportCompare(expect, actual, summary);
