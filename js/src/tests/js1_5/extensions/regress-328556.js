/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 328556;
var summary = 'Do not Assert: growth == (size_t)-1 || (nchars + 1) * sizeof(char16_t) == growth, in jsarray.c';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

var D = [];
D.foo = D;
uneval(D);
 
reportCompare(expect, actual, summary);
