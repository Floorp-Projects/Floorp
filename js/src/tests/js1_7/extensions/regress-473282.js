/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 473282;
var summary = 'Do not assert: thing';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
this.watch("b", "".substring);
this.__defineGetter__("a", gc);
for each (b in [this, null, null]);
a;

reportCompare(expect, actual, summary);
