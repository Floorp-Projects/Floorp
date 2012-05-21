/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
// have not been able to reproduce the crash in any build.
var BUGNUMBER = 260541;
var summary = 'Recursive Error object should not crash';
var actual = 'Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
var myErr = new Error( "Error Text" );
myErr.name = myErr;

actual = 'No Crash';

reportCompare(expect, actual, summary);
