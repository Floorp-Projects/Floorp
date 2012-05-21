/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 407019;
var summary = 'Do not assert: !JS_IsExceptionPending(cx) - Browser only';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

if (typeof window != 'undefined' && typeof window.Option == 'function' && '__proto__' in window.Option)
{
  try
  {
    expect = /Illegal operation on WrappedNative prototype object/;
    window.Option("u", window.Option.__proto__);
    for (p in document) { }
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportMatch(expect, actual, summary);
}
else
{
  expect = actual = 'Test can only run in a Gecko 1.9 browser or later.';
  print(actual);
  reportCompare(expect, actual, summary);
}


