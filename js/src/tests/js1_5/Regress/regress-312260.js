/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 312260;
var summary = 'Switch discriminant detecting case should not warn';
var actual = 'No warning';
var expect = 'No warning';

printBugNumber(BUGNUMBER);
printStatus (summary);

options('strict');
options('werror');

try
{
  switch ({}.foo) {}
}
catch(e)
{
  actual = e + '';
}

reportCompare(expect, actual, summary);
