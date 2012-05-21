/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 319384;
var summary = 'Do not crash converting string to number';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

printStatus('This test only runs in the browser');

if (typeof clearTimeout === 'function')
{
  try
  {
    clearTimeout('foo');
  }
  catch(ex)
  {
  }
}
 
reportCompare(expect, actual, summary);
