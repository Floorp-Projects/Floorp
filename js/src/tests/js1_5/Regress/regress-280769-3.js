/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 280769;
var summary = 'Do not crash on overflow of 64K boundary in number of classes in regexp';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

var N = 100 * 1000;

status = summary + ' ' + inSection(3) + ' (new RegExp("[0][1]...[99999]").exec("") ';

var a = new Array(N);

for (var i = 0; i != N; ++i) {
  a[i] = i;
}

var str = '['+a.join('][')+']'; // str is [0][1][2]...[<PRINTED N-1>]

try
{
  var re = new RegExp(str);
}
catch(e)
{
  printStatus('Exception creating RegExp: ' + e);
}

try
{
  re.exec('');
}
catch(e)
{
  printStatus('Exception executing RegExp: ' + e);
}

reportCompare(expect, actual, status);
