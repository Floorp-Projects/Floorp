/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 306727;
var summary = 'Parsing RegExp of octal expressions in strict mode';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

// test with strict off

try
{
  expect = null;
  actual = /.\011/.exec ('a'+String.fromCharCode(0)+'11');
}
catch(e)
{
}

reportCompare(expect, actual, summary);

// test with strict on
options('strict');

expect = null;
try
{
  actual = /.\011/.exec ('a'+String.fromCharCode(0)+'11');
}
catch(e)
{
}

reportCompare(expect, actual, summary);
