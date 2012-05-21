/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 176125;
var summary = 'if() should not return a value';
var actual = '';
var expect = 'undefined';

printBugNumber(BUGNUMBER);
printStatus (summary);

try
{
  printStatus('if (test1());');
  actual = typeof eval('if (test1());');
}
catch(ex)
{
  actual = ex + '';
}
 
reportCompare(expect, actual, summary + ': if (test1());');

try
{
  printStatus('if (test2());');
  actual = typeof eval('if (test2());');
}
catch(ex)
{
  actual = ex + '';
}
 
reportCompare(expect, actual, summary + ': if (test2());');


function test1()
{
  'Hi there!';
}

function test2()
{
  test1();
  return false;
}
