/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 311497;
var summary = 'Root pivots in js_HeapSort';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);


function force_gc()
{
  if (this.gc) gc();
  for (var i = 0; i != 30000; ++i) {
    var tmp = Math.sin(i);
    tmp = null;
  }
}

var array = new Array(10);
for (var i = 0; i != array.length; ++i) {
  array[i] = String.fromCharCode(i, i, i);
}

function cmp(a, b)
{
  for (var i = 0; i != array.length; ++i) {
    array[i] = null;
  }
  force_gc();
  return 0;
}

array.sort(cmp);

// Verify that array contains either null or original strings

var null_count = 0;
var original_string_count = 0;
for (var i = 0; i != array.length; ++i) {
  var elem = array[i];
  if (elem === null) {
    ++null_count;
  } else if (typeof elem == "string" && elem.length == 3) {
    var code = elem.charCodeAt(0);
    if (0 <= code && code < array.length) {
      if (code === elem.charCodeAt(1) && code == elem.charCodeAt(2))
	++original_string_count;
    }
  }
}

var expect = array.length;
var actual = null_count + original_string_count;

reportCompare(expect, actual, summary);
