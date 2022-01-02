/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 140852;
var summary = 'String(number) = xxxx:0000 for some numbers';
var actual = '';
var expect = '';


printBugNumber(BUGNUMBER);
printStatus (summary);

var value;
 
value = 99999999999;
expect = '99999999999';
actual = value.toString();
reportCompare(expect, actual, summary);

value = 100000000000;
expect = '100000000000';
actual = value.toString();
reportCompare(expect, actual, summary);

value = 426067200000;
expect = '426067200000';
actual = value.toString();
reportCompare(expect, actual, summary);

