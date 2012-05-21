/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 453051;
var summary = 'Do not assert with JIT: !(((*pc == JSOP_GOTO) || (*pc == JSOP_GOTOX)) && (exitType != LOOP_EXIT))';
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

  for (var p in this){} for (let a in [5,6,7]) for (var b=0;b<1;++b) break;

  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
