// |reftest| skip-if(!xulRuntime.shell) -- bug xxx - fails to dismiss alert
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 341815;
var summary = 'Close hook crash';
var actual = 'No Crash';
var expect = 'No Crash';

var ialert = 0;
//-----------------------------------------------------------------------------
//test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  var globalToPokeGC = {};

  function make_iterator()
  {
    function generator() {
      try {
        yield 0;
      } finally {
        make_iterator();
      }
    }
    generator().next();
    globalToPokeGC = {};
    if (typeof alert != 'undefined')
    {
      alert(++ialert);
    }
  }

  make_iterator();

  for (var i = 0; i != 50000; ++i) {
    var x = {};
  }
 
  print('done');

  setTimeout('checkTest()', 10000);

  exitFunc ('test');
}

function init()
{
  // give the dialog closer time to register
  setTimeout('test()', 5000);
}

var lastialert = 0;

function checkTest()
{
  // this function is used to check if there
  // additional alerts are still being fired 
  // in order to prevent the test from completing
  // until all alerts have finished.

  if (ialert != lastialert)
  {
    lastialert = ialert;
    setTimeout('checkTest()', 10000);
    return;
  }

  reportCompare(expect, actual, summary);
  gDelayTestDriverEnd = false;
  jsTestDriverEnd();
}

if (typeof window != 'undefined')
{
  // delay test driver end
  gDelayTestDriverEnd = true;

  window.addEventListener("load", init, false);
}
else
{
  reportCompare(expect, actual, summary);
}

