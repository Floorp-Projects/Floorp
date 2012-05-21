// |reftest| skip -- bogus perf test (bug 540512)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 469927;
var summary = 'TM: jit should not slow down short loop with let';
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
 
  function letitbe() {
    var start = new Date();
    for (let i = 0; i < 500000; ++i) { 
      for (let j = 0; j < 4; ++j) { } 
    }
    var stop = new Date();
    return stop - start;
  }

  jit(false);
  var timenonjit = letitbe();
  jit(true);
  var timejit = letitbe();
  jit(false);

  print('time: nonjit = ' + timenonjit + ', jit = ' + timejit);

  expect = true;
  actual = timejit < timenonjit;

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
