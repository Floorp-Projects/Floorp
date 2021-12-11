/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 310425;
var summary = 'Array.indexOf/lastIndexOf edge cases';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
expect = -1;
actual = Array(1).indexOf(1);
reportCompare(expect, actual, summary);
