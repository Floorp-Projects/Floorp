// |reftest| random -- bogus perf test (bug 467263)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.

/*
 * Date: 27 September 2001
 *
 * SUMMARY: Performance: truncating even very large arrays should be fast!
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=101964
 *
 * Adjust this testcase if necessary. The FAST constant defines
 * an upper bound in milliseconds for any truncation to take.
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 101964;
var summary = 'Performance: truncating even very large arrays should be fast!';
var BIG = 10000000;
var LITTLE = 10;
var FAST = 50; // array truncation should be 50 ms or less to pass the test
var MSG_FAST = 'Truncation took less than ' + FAST + ' ms';
var MSG_SLOW = 'Truncation took ';
var MSG_MS = ' ms';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];



status = inSection(1);
var arr = Array(BIG);
var start = new Date();
arr.length = LITTLE;
actual = elapsedTime(start);
expect = FAST;
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function elapsedTime(startTime)
{
  return new Date() - startTime;
}


function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = isThisFast(actual);
  expectedvalues[UBound] = isThisFast(expect);
  UBound++;
}


function isThisFast(ms)
{
  if (ms <= FAST)
    return MSG_FAST;
  return MSG_SLOW + ms + MSG_MS;
}


function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
