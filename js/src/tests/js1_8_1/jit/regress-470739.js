// |reftest| skip -- bogus perf test (bug 540512)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 470739;
var summary = 'TM: never abort on ==';
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

  function loop()
  {
    var i;
    var start = new Date();

    for(i=0;i<500000;++i) { var r = (void 0) == null; }

    var stop = new Date();
    return stop - start;
  }

  var timenonjit = loop();
  var timejit = loop();

  print('time: nonjit = ' + timenonjit + ', jit = ' + timejit);

  expect = true;
  actual = timejit < timenonjit;

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
