/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 281930;
var summary = 'this reference should point to global object in function expressions';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var global = this;

actual = function() { return this; }();
expect = global;
 
reportCompare(expect, actual, summary);
