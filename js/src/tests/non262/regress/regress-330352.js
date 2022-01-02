// |reftest| skip-if(Android)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 330352;
var summary = 'Very non-greedy regexp causes crash in jsregexp.c';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

expectExitCode(0);
expectExitCode(5);

if ("AB".match(/(.*?)*?B/))
{
  printStatus(RegExp.lastMatch);
}
reportCompare(expect, actual, summary + ': "AB".match(/(.*?)*?B/)');

if ("AB".match(/(.*)*?B/))
{
  printStatus(RegExp.lastMatch);
}
reportCompare(expect, actual, summary + ': "AB".match(/(.*)*?B/)');

if ("AB".match(/(.*?)*B/))
{
  printStatus(RegExp.lastMatch);
}
reportCompare(expect, actual, summary + ': "AB".match(/(.*?)*B/)');
