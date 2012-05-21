/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 316885;
var summary = 'Unrooted access in jsinterp.c';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var str = "test";

var lval = {
  valueOf: function() {
    return str+"1";
  }
};

var ONE = 1;

var rval = {
  valueOf: function() {
    // Make sure that the result of the previous lval.valueOf
    // is not GC-rooted.
    var tmp = "x"+lval;
    if (typeof gc == "function")
      gc();
    for (var i = 0; i != 40000; ++i) {
      tmp = 1e100 / ONE;
    }
    return str;
  }
};

expect = (str+"1")+str;
actual = lval+rval;

reportCompare(expect, actual, summary);
