/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 294302;
var summary = 'JS Shell load should throw catchable exceptions';
var actual = 'Error not caught';
var expect = 'Error caught';

printBugNumber(BUGNUMBER);
printStatus (summary);

try
{
  load('foo.js');
}
catch(ex)
{
  actual = 'Error caught';
  printStatus(actual);
}
reportCompare(expect, actual, summary);
