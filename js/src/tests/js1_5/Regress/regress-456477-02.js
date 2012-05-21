/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 456477;
var summary = 'Do not assert with JIT: (m != JSVAL_INT) || isInt32(*vp)" with (0/0)%(-1)';
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

  (function() { for (var j = 0; j < 5; ++j) { (0 / 0) % (-1); } })();

  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
