/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER     = "(none)";
var summary = "Order of destructuring, destructuring in the presence of " +
  "exceptions";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;


var a = "FAILED", b = "PASSED";

function exceptObj()
{
  return { get b() { throw "PASSED"; }, a: "PASSED" };
}

function partialEvalObj()
{
  try
  {
    ({a:a, b:b} = exceptObj());
    throw "FAILED";
  }
  catch (ex)
  {
    if (ex !== "PASSED")
      throw "bad exception thrown: " + ex;
  }
}


var c = "FAILED", d = "FAILED", e = "PASSED", f = "PASSED";

function exceptArr()
{
  return ["PASSED", {e: "PASSED", get f() { throw "PASSED"; }}, "FAILED"];
}

function partialEvalArr()
{
  try
  {
    [c, {e: d, f: e}, f] = exceptArr();
    throw "FAILED";
  }
  catch (ex)
  {
    if (ex !== "PASSED")
      throw "bad exception thrown: " + ex;
  }
}


var g = "FAILED", h = "FAILED", i = "FAILED", j = "FAILED", k = "FAILED";
var _g = "PASSED", _h = "FAILED", _i = "FAILED", _j = "FAILED", _k = "FAILED";
var order = [];

function objWithGetters()
{
  return {
    get j()
    {
      var rv = _j;
      _g = _h = _i = _j = "FAILED";
      _k = "PASSED";
      order.push("j");
      return rv;
    },
      get g()
      {
	var rv = _g;
	_g = _i = _j = _k = "FAILED";
	_h = "PASSED";
	order.push("g");
	return rv;
      },
	get i()
	{
	  var rv = _i;
	  _g = _h = _i = _k = "FAILED";
	  _j = "PASSED";
	  order.push("i");
	  return rv;
	},
	  get k()
	  {
	    var rv = _k;
	    _g = _h = _i = _j = _k = "FAILED";
	    order.push("k");
	    return rv;
	  },
	    get h()
	    {
	      var rv = _h;
	      _g = _h = _j = _k = "FAILED";
	      _i = "PASSED";
	      order.push("h");
	      return rv;
	    }
  };
}

function partialEvalObj2()
{
  ({g: g, h: h, i: i, j: j, k: k} = objWithGetters());
}

try
{
  partialEvalObj();
  if (a !== "PASSED" || b !== "PASSED")
    throw "FAILED: lhs not mutated correctly during destructuring!\n" +
      "a == " + a + ", b == " + b;

  partialEvalObj2();
  if (g !== "PASSED" ||
      h !== "PASSED" ||
      i !== "PASSED" ||
      j !== "PASSED" ||
      k !== "PASSED")
    throw "FAILED: order of property accesses wrong!\n" +
      "order == " + order;

  partialEvalArr();
  if (c !== "PASSED" || d !== "PASSED" || e !== "PASSED")
    throw "FAILED: lhs not mutated correctly during destructuring!\n" +
      "c == " + c +
      ", d == " + d +
      ", e == " + e +
      ", f == " + f ;
}
catch (ex)
{
  failed = ex;
}

expect = false;
actual = failed;

reportCompare(expect, actual, summary);
