// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 106244;
var summary = 'No warning in strict mode if (a = b && c == d)...';
var actual = '';
var expect = 'test for equality (==) mistyped as assignment (=)?';

printBugNumber(BUGNUMBER);
printStatus (summary);

options('strict');
options('werror');

var a = false;
var b = true;
var c = false;
var d = true;
var result;

try
{
  if (a = b && c == d)
    result = true;
  else
    result = false;
}
catch(ex)
{
  actual = ex.message;
}

print('result = ' + result);

reportCompare(expect, actual, summary);
