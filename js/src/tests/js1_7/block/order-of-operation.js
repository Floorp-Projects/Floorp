/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER     = "(none)";
var summary = "Test let and order of operation issues";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

function f1(x)
{
  let (foo) {
    // scope of lhs x includes rhs, so x is NaN here -- bug 344952
    let x = ++x;
    return x;
  }
}

function f2(x)
{
  let (foo)
  {
    // scope of lhs x includes rhs, so x is NaN here -- bug 344952
    let x = x++;
    return x;
  }
}

function f3(x)
{
  let (foo)
  {
    var q = x;
    let (x = x++)
    {
      if (x != q)
        throw "f3():\n" +
          "  expected: x == q\n" +
          "  actual:   x != q " +
          "(where x == " + x + ", q == " + q + ")\n";
    }
    return x;
  }
}

function f4(x)
{
  var y = 7;
  let (y = x, x = 3)
  {
    var q = 7 + x;
  }
  return x + y + q;
}

function f5(x)
{
  var q = x++;
  let (y = x, r = 17, m = 32) {
    return function(code)
    {
      return eval(code);
    };
  }
}

function f6() {
  let (foo)
  {
    var i=3;
    for (let i=i;;) { if (i != 3) throw "f6(): fail 1"; i = 7; break; }
    if (i != 3) throw "f6(): fail 2";
  }
}

try
{
  var rv = f1(5);
  if (!isNaN(rv))
    throw "f1(5):\n" +
      "  expected:  NaN\n" +
      "  actual:    " + rv;

  rv = f2(5);
  if (!isNaN(rv))
    throw "f2(5):\n" +
      "  expected:  NaN\n" +
      "  actual:    " + rv;
/*
  rv = f3(8);
  if (rv != 9)
  throw "f3(8):\n" +
  "  expected:  9\n" +
  "  actual:    " + rv;
*/

  rv = f4(13);
  if (rv != 30)
    throw "f4(13):\n" +
      "  expected:  30\n" +
      "  actual:    " + rv;

  var fun = f5(2);

  rv = fun("q");
  if (rv != 2)
    throw "fun('q'):\n" +
      "  expected:  2\n" +
      "  actual:    " + rv;

  rv = fun("x");
  if (rv != 3)
    throw "fun('x'):\n" +
      "  expected:  3\n" +
      "  actual:    " + rv;

  rv = fun("y");
  if (rv != 3)
    throw "fun('y'):\n" +
      "  expected:  3\n" +
      "  actual:    " + rv;

  rv = fun("let (y = y) { y += 32; }; y");
  if (rv != 3)
    throw "fun('let (y = y) { y += 32; }; y'):\n" +
      "  expected:  3\n" +
      "  actual:    " + rv;

/*
  f6();
*/
}
catch (e)
{
  print(e.toSource());
  failed = e;
}

expect = false;
actual = failed;

reportCompare(expect, actual, summary);
