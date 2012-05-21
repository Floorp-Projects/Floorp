/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 481989;
var summary = 'TM: Do not assert: SPROP_HAS_STUB_SETTER(sprop)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

jit(true);

y = this.watch("x", function(){}); for each (let y in ['', '']) x = y;

jit(true);

reportCompare(expect, actual, summary);
