// |reftest| skip -- slow, killed on x86_64
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 338653;
var summary = 'Force GC when JSRuntime.gcMallocBytes hits ' +
  'JSRuntime.gcMaxMallocBytes';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);
print('This test should never fail explicitly. ' +
      'You must view the memory usage during the test. ' +
      'This test fails if the memory usage repeatedly spikes ' +
      'by several hundred megabytes.');

function dosubst()
{
  var f = '0x';
  var s = f;

  for (var i = 0; i < 18; i++)
  {
    s += s;
  }

  var index = s.indexOf(f);
  while (index != -1 && index < 10000) {
    s = s.substr(0, index) + '1' + s.substr(index + f.length);
    index = s.indexOf(f);
  }

}

dosubst(); 

reportCompare(expect, actual, summary);
