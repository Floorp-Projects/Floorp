/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 287630;
var summary = '15.5.4.14 - String.prototype.split(/()/)';
var actual = '';
var expect = ['a'].toString();

printBugNumber(BUGNUMBER);
printStatus (summary);

actual = 'a'.split(/()/).toString();
 
reportCompare(expect, actual, summary);
