/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 115436;
var summary = 'Do not crash javascript warning duplicate arguments';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

options('strict');

function x(y,y)
{
  return 3;
}

var z = x(4,5);

reportCompare(expect, actual, summary);
