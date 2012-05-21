/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = "446494";
var summary = "num.toLocaleString should handle exponents";
var actual, expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

expect = '1e-10';
actual = 1e-10.toLocaleString();
reportCompare(expect, actual, summary + ': ' + expect);

expect = 'Infinity';
actual = Infinity.toLocaleString();
reportCompare(expect, actual, summary + ': ' + expect);
