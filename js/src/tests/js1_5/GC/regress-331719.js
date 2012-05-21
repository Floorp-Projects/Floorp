/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 331719;
var summary = 'Problem with String.replace running with WAY_TOO_MUCH_GC';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
print('This test requires WAY_TOO_MUCH_GC');
 
expect = 'No';
actual = 'No'.replace(/\&\&/g, '&');

reportCompare(expect, actual, summary);
