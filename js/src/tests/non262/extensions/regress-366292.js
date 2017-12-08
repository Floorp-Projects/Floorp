/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 366292;
var summary = '__defineSetter__ and JSPROP_SHARED regression';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

expect = 'undefined';
this.__defineSetter__("x", function(){});
actual = String(x);

reportCompare(expect, actual, summary);
