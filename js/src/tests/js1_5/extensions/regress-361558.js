/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 361558;
var summary = 'Do not assert: sprop->setter != js_watch_set';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
expect = actual = 'No Crash';

({}.__proto__.watch('x', print)); ({}.watch('x', print));

reportCompare(expect, actual, summary);
