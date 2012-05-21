/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 257751;
var summary = 'RegExp Syntax Errors should have lineNumber and fileName';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var status;
var re;

status = summary + ' ' + inSection(1) + ' RegExp("\\\\") ';
try
{
  expect = 'Pass';
  re = RegExp('\\');
}
catch(e)
{
  if (e.fileName && e.lineNumber)
  {
    actual = 'Pass';
  }
  else
  {
    actual = 'Fail';
  }
}
reportCompare(expect, actual, status);

status = summary + ' ' + inSection(2) + ' RegExp(")") ';
try
{
  expect = 'Pass';
  re = RegExp(')');
}
catch(e)
{
  if (e.fileName && e.lineNumber)
  {
    actual = 'Pass';
  }
  else
  {
    actual = 'Fail';
  }
}
reportCompare(expect, actual, status);

status = summary + ' ' + inSection(3) + ' /\\\\/ ';
try
{
  expect = 'Pass';
  re = eval('/\\/');
}
catch(e)
{
  if (e.fileName && e.lineNumber)
  {
    actual = 'Pass';
  }
  else
  {
    actual = 'Fail';
  }
}
reportCompare(expect, actual, status);

status = summary + ' ' + inSection(4) + ' /)/ ';
try
{
  expect = 'Pass';
  re = eval('/)/');
}
catch(e)
{
  if (e.fileName && e.lineNumber)
  {
    actual = 'Pass';
  }
  else
  {
    actual = 'Fail';
  }
}
reportCompare(expect, actual, status);
