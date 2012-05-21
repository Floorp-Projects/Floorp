/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 356402;
var summary = 'Do not assert: slot < fp->nvars';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

if (typeof Script == 'undefined')
{
  print('Test skipped. Script not defined.');
}
else
{ 
  (function() { new Script('for(var x in x) { }')(); })();
}
reportCompare(expect, actual, summary);
