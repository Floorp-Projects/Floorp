/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 317476;
var summary = 'The error thrown by JS_ReportError should be catchable';
var actual = 'no error';
var expect = 'no error';

printBugNumber(BUGNUMBER);
printStatus (summary);

if (typeof setTimeout != 'undefined')
{
  expect = 'error';
  try
  {
    setTimeout(2);
  }
  catch(ex)
  {
    actual = 'error';
    printStatus(ex+'');
  }
}

 

reportCompare(expect, actual, summary);
