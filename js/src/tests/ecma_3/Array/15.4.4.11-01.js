/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 312138;
var summary = 'Array.sort should not eat exceptions';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

expect = "e=1 N=1";

var N = 0;
var array = [4,3,2,1];

try {
  array.sort(function() {
	       throw ++N;
	     });
} catch (e) {
  actual = ("e="+e+" N="+N);
}

reportCompare(expect, actual, summary);
