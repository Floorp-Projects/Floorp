// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 421621;
var summary = 'Do not assert with setter, export/import: (sprop)->slot != SPROP_INVALID_SLOT || !SPROP_HAS_STUB_SETTER(sprop)';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
var th = this;
this.__defineSetter__('x', function () {});
export *;
import th.*;
x;

reportCompare(expect, actual, summary);
