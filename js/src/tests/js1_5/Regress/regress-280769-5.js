/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 280769;
var summary = 'Do not overflow 64K string offset';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

var N = 100 * 1000;

var prefix = new Array(N).join("a"); // prefix is sequence of N "a"

var suffix = "111";

var re = new RegExp(prefix+"0?"+suffix);  // re is /aaa...aaa0?111/

var str_to_match = prefix+suffix;  // str_to_match is "aaa...aaa111"

try {
  var index = str_to_match.search(re);

  expect = 0;
  actual = index;

  reportCompare(expect, actual, summary);
} catch (e) {
  reportCompare(true, e instanceof Error, actual, summary);
}
