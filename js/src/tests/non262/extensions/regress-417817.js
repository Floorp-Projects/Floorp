// |reftest| skip
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 417817;
var summary = 'Do not assert: ASSERT_VALID_PROPERTY_CACHE_HIT';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

const numThreads = 2;
const numPasses = 1000;

var tests = {
  0: function first(myAn)
  {
    /* This print statement is needed to reliably trigger the bug */
    print("Hello, World!");
  },
  length: 1
};

function runAllTests()
{
  var passes;
  var i;

  for (passes = 0; passes < numPasses; passes++)
  {
    for (i = 0; i < tests.length; i++)
    {
      tests[0]();
    }
  }
}

if (typeof scatter == 'undefined')
{
  print(expect = actual = 'Test skipped. Requires scatter.');
}
else
{
  var i;
  var a = [];
  for (i = 0; i < numThreads; i++)
    a.push(runAllTests);
  scatter(a);
}

reportCompare(expect, actual, summary);
