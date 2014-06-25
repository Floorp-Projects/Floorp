/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER     = "344139";
var summary = "Basic let functionality";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

var x = 7;

function f1()
{
  let x = 5;
  return x;
}

function f2()
{
  let x = 5;
  x = 3;
  return x;
}

function f3()
{
  let x = 5;
  x += x;
  return x;
}

function f4()
{
  var v = 5;
  let q = 17;

  // 0+1+2+...+8+9 == 45
  for (let v = 0; v < 10; v++)
    q += v;

  if (q != 62)
    throw "f4(): wrong value for q\n" +
      "  expected: 62\n" +
      "  actual:   " + q;

  return v;
}

try
{
  if (f1() != 5 || x != 7)
    throw "f1() == 5";
  if (f2() != 3 || x != 7)
    throw "f2() == 3";
  if (f3() != 10 || x != 7)
    throw "f3() == 10";

  if (f4() != 5)
    throw "f4() == 5";

  var bad = true;
  try
  {
    eval("q++"); // force error at runtime
  }
  catch (e)
  {
    if (e instanceof ReferenceError)
      bad = false;
  }
  if (bad)
    throw "f4(): q escaping scope!";
}
catch (e)
{
  failed = e;
}

expect = false;
actual = failed;

reportCompare(expect, actual, summary);
