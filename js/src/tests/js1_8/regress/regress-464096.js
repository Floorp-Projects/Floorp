/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 464096;
var summary = 'TM: Do not assert: tm->recoveryDoublePoolPtr > tm->recoveryDoublePool';
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

  jit(true);

  for (let f in [1,1]);
  Object.prototype.__defineGetter__('x', function() gc());
  (function() { for each (let j in [1,1,1,1,1]) { var y = .2; } })();

  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
