/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 341939;
var summary = 'Let block does not require semicolon';
var actual = '';
var expect = 'No Error';

printBugNumber(BUGNUMBER);
printStatus (summary);

try
{ 
  eval('let (a) {} print(42);');
  actual = 'No Error';
}
catch(ex)
{
  actual = ex + '';
}

reportCompare(expect, actual, summary);
