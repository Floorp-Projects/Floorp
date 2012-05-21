/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 453173;
var summary = 'Do not Crash with JIT [@ TraceRecorder::record_JSOP_ENDINIT] with "[,]"';
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

  var i;

  jit(true);

  for(i=0;i<4;++i) [,];

  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
