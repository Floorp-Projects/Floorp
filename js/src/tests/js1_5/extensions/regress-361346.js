/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 361346;
var summary = 'Crash with setter, watch, GC';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
expect = actual = 'No Crash';

Object.defineProperty(this, "x", { set: new Function, enumerable: true, configurable: true });
this.watch('x', function(){});
gc();
x = {};

reportCompare(expect, actual, summary);
