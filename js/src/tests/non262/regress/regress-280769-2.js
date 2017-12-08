// |reftest| skip-if(Android) silentfail
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 280769;
var summary = 'Do not overflow 64K boundary in treeDepth';
var actual = 'No Crash';
var expect = /No Crash|InternalError: allocation size overflow/;
var status;
var result;

printBugNumber(BUGNUMBER);
printStatus (summary);

expectExitCode(0);
expectExitCode(5);

status = summary + ' ' + inSection(1) + ' (new RegExp("0|...|99999") ';

try
{
  var N = 100 * 1000;
  var a = new Array(N);
  for (var i = 0; i != N; ++i) {
    a[i] = i;
  }
  var str = a.join('|');  // str is 0|1|2|3|...|<printed value of N -1>
  var re = new RegExp(str);
  re.exec(N - 1);
}
catch(ex)
{
  actual = ex + '';
}

print('Done: ' + actual);

reportMatch(expect, actual, summary);



