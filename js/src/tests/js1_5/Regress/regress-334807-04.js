/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 334807;
var summary = '10.1.8 - arguments prototype is the original Object prototype.';
var actual = 'No Error';
var expect = 'TypeError';

printBugNumber(BUGNUMBER);
printStatus (summary);

printStatus('set Object = Array');

Object = Array;

try
{
  0, function () { printStatus( arguments.join()); }( 1, 2, 3 ); 
}
catch(ex)
{
  printStatus(ex + '');
  actual = ex.name;
}
reportCompare(expect, actual, summary);
