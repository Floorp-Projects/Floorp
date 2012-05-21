// |reftest| skip-if(!xulRuntime.shell) -- bug xxx - fails to dismiss alert
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 341821;
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

  function generator()
  {
    try {
      yield [];
    } finally {
      make_iterator();
    }
  }

  function make_iterator()
  {
    var iter = generator();
    iter.next();
    iter = null;
    if (typeof alert != 'undefined')
    {
      alert(++ialert);
    }
  }

  make_iterator();

  // Trigger GC through the branch callback.
  for (var i = 0; i != 50000; ++i) {
    var x = {};
  }

  print('done');
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}

function init()
{
  // give the dialog closer time to register
  setTimeout('runtest()', 5000);
}

function runtest()
{
  test();
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

