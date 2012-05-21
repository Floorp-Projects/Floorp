/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 452178;
var summary = 'Do not assert with JIT: !(sprop->attrs & JSPROP_SHARED)';
var actual = 'No Crash';
var expect = 'No Crash';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  jit(true);

  Object.defineProperty(this, "q", { get: function(){}, enumerable: true, configurable: true });
  for (var j = 0; j < 4; ++j) q = 1;

  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
