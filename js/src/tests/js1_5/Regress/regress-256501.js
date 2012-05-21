/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 256501;
var summary = 'Check Recursion';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

expect = 'error';

try
{ 
  var N = 100*1000;
  var buffer = new Array(N * 2 + 1);
  for (var i = 0; i != N; ++i)
  {
    buffer[i] = 'do ';
    buffer[buffer.length - i - 1] = ' while(0);';
  }
  buffer[N] = 'printStatus("TEST");';

  // text is do do ... do print("TEST"); while(0); while(0); ... while(0);
  var text = buffer.join('');

  eval(text);
  actual = 'no error';
}
catch(e)
{
  actual = 'error';
}
reportCompare(expect, actual, summary);
