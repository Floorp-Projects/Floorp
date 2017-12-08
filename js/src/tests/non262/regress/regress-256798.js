/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 256798;
var summary = 'regexp zero-width positive lookahead';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var status;

status = summary + ' ' + inSection(1);
expect = 'aaaa,a';
actual = /(?:(a)+)/.exec("baaaa") + '';
reportCompare(expect, actual, status);

status = summary + ' ' + inSection(2);
expect = ',aaa';
actual = /(?=(a+))/.exec("baaabac") + '';
reportCompare(expect, actual, status);

status = summary + ' ' + inSection(3);
expect = 'b,aaa';
actual = /b(?=(a+))/.exec("baaabac") + '';
reportCompare(expect, actual, status);

// XXXbc revisit this
status = summary + ' ' + inSection(4);
expect = 'null';
actual = /b(?=(b+))/.exec("baaabac") + '';
reportCompare(expect, actual, status);
