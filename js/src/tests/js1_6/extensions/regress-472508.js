/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 472508;
var summary = 'TM: Do not crash @ TraceRecorder::emitTreeCall';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

jit(true);
 
for (var x in this) { }
var a = [false, false, false];
a.__defineGetter__("q", function() { });
a.__defineGetter__("r", function() { });
for (var i = 0; i < 2; ++i) for each (var e in a) { }

jit(false);

reportCompare(expect, actual, summary);
