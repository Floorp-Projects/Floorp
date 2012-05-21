/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 453249;
var summary = 'Do not assert with JIT: s0->isQuad()';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

jit(true);

this.__proto__.a = 3; for (var j = 0; j < 4; ++j) { [a]; }

this.a = 3; for (var j = 0; j < 4; ++j) { [a]; }

jit(false);

reportCompare(expect, actual, summary);
