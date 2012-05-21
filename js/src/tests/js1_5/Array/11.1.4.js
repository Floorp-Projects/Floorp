/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 260106;
var summary = 'Elisons in Array literals should not be enumed';
var actual = '';
var expect = '';
var status;
var prop;
var array;

printBugNumber(BUGNUMBER);
printStatus (summary);

status = summary + ' ' + inSection(1) + ' [,1] ';
array = [,1];
actual = '';
expect = '1';
for (prop in array)
{
  if (prop != 'length')
  {
    actual += prop;
  }
}
reportCompare(expect, actual, status);
 
status = summary + ' ' + inSection(2) + ' [,,1] ';
array = [,,1];
actual = '';
expect = '2';
for (prop in array)
{
  if (prop != 'length')
  {
    actual += prop;
  }
}
reportCompare(expect, actual, status);

status = summary + ' ' + inSection(3) + ' [1,] ';
array = [1,];
actual = '';
expect = '0';
for (prop in array)
{
  if (prop != 'length')
  {
    actual += prop;
  }
}
reportCompare(expect, actual, status);
 
status = summary + ' ' + inSection(4) + ' [1,,] ';
array = [1,,];
actual = '';
expect = '0';
for (prop in array)
{
  if (prop != 'length')
  {
    actual += prop;
  }
}
reportCompare(expect, actual, status);
