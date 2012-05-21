/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 369696;
var summary = 'Do not assert: map->depth > 0" in js_LeaveSharpObject';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  function fun() {}
  n = fun.prototype;
  n.__defineGetter__("prototype", n.toSource);
  p = n.__lookupGetter__("prototype");
  n = p;

  assertEq(n, Object.prototype.toSource);
  assertEq(p, Object.prototype.toSource);

  n["prototype"] = [n];
  n = p;

  assertEq(n, Object.prototype.toSource);
  assertEq(p, Object.prototype.toSource);

  p2 = n["prototype"];

  assertEq(Array.isArray(p2), true);
  assertEq(p2[0], Object.prototype.toSource);

  n = p2;

  assertEq(n.toString, Array.prototype.toString);
  n.__defineGetter__("0", n.toString);
  n = p;

  assertEq(n, Object.prototype.toSource);

  n.call(this);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
