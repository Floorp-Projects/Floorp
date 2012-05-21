/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
// test from Henrik Gemal
var BUGNUMBER = 243389;
var summary = 'Don\'t crash on Regular Expression';
var actual = 'Crash';
var expect = 'error';

printBugNumber(BUGNUMBER);
printStatus (summary);

// this is a syntax error which will fire
// before the framework is ready. therefore the browser based
// version will not appear to run this test if it does not crash
if (/(\\|/)/) {
}

actual = 'No Crash';

reportCompare(expect, actual, summary);
