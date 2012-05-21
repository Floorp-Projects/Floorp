// |reftest| skip-if(Android) -- bug - nsIDOMWindow.crypto throws NS_ERROR_NOT_IMPLEMENTED on Android
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 455464;
var summary = 'Do not assert with JIT, gczeal 2: !TRACE_RECORDER(cx) ^ (jumpTable == recordingJumpTable)';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

if (typeof gczeal == 'undefined')
{
  expect = actual = 'Test requires gczeal, skipped.';
}
else
{
  jit(true);
  gczeal(2);

  a=b=c=d=0; this.__defineGetter__('g', gc); for each (y in this);

  gczeal(0);
  jit(false);
}

reportCompare(expect, actual, summary);
