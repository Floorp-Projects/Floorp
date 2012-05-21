/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 305002;
var summary = '[].every(f) == true';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var notcalled = true;

function callback()
{
  notcalled = false;
}

expect = true;
actual = [].every(callback) && notcalled;

reportCompare(expect, actual, summary);
