/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 474771;
var summary = 'TM: do not assert: jumpTable == interruptJumpTable';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

jit(true);

this.__defineSetter__('x', function(){});
for (var j = 0; j < 5; ++j) { x = 3; }

jit(false);

reportCompare(expect, actual, summary);
