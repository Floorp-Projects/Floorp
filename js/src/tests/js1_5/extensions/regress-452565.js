/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 452565;
var summary = 'Do not assert with JIT: !(sprop->attrs & JSPROP_READONLY)';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

jit(true);

const c = undefined; (function() { for (var j=0;j<5;++j) { c = 1; } })();

jit(false);

reportCompare(expect, actual, summary);
