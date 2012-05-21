/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 375639;
var summary = 'Uneval|Decompilation of string containing null character';
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
 
  print('normalize \\x00 to \\0 to hide 1.8.1 vs 1.9.0 ' +
        'branch differences.');

  expect = '"a\\' + '0b"';
  actual =  uneval('a\0b').replace(/\\x00/g, '\\0');
  reportCompare(expect, actual, summary + ': 1');

  expect = 'function () { return "a\\0b"; }';
  actual = ((function () { return "a\0b"; }) + '').replace(/\\x00/g, '\\0');;
  compareSource(expect, actual, summary + ': 2');

  exitFunc ('test');
}
