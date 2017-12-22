/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 312354;
var summary = '11.13.1 Simple Assignment should return type of RHS';
var actual = '';
var expect = '';

// XXX this test should really test each property of the native
// objects, but I'm too lazy. Patches accepted.

printBugNumber(BUGNUMBER);
printStatus (summary);

var re = /x/g;
var y = re.lastIndex = "7";
 
expect = "string";
actual = typeof y;

reportCompare(expect, actual, summary);
