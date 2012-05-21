/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 334807;
var summary = '12.14 - exception prototype is the original Object prototype.';
var actual = 'No Error';
var expect = 'ReferenceError';

printBugNumber(BUGNUMBER);
printStatus (summary);

printStatus('set Error = Number');

Error = Number;

try
{
  x.y;
}
catch(ex)
{
  try
  {
    actual = ex.name;
    printStatus(ex + ': x.y');
    ex.valueOf();
  }
  catch(ex2)
  {
    printStatus(ex2 + ': ex.valueOf()');
    actual = ex2.name;
  }
}
reportCompare(expect, actual, summary);
