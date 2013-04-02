// |reftest| skip-if(Android)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 312588;
var summary = 'Do not crash creating infinite array';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus(summary);

var a = new Array();

try
{
  for (var i = 0; i < 1e6; i++)
    (a = new Array(a)).sort();
}
catch(ex)
{
}

reportCompare(expect, actual, summary);
