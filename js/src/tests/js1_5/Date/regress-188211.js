/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 188211;
var summary = 'Date.prototype.toLocaleString() error on future dates';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var dt;

dt = new Date(208e10);
printStatus(dt+'');
expect = true;
actual = dt.toLocaleString().indexOf('2035') >= 0;
reportCompare(expect, actual, summary + ': new Date(208e10)');

dt = new Date(209e10);
printStatus(dt+'');
expect = true;
actual = dt.toLocaleString().indexOf('2036') >= 0;
reportCompare(expect, actual, summary + ': new Date(209e10)');
