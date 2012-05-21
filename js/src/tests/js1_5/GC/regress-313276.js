/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 313276;
var summary = 'Root strings';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
var obj = {
  toString: function() {
    return "*TEST*".substr(1, 4);
  }
};

var TMP = 1e200;

var likeZero = {
  valueOf: function() {
    if (typeof gc == "function") gc();
    for (var i = 0; i != 40000; ++i) {
      var tmp = 2 / TMP;
      tmp = null;
    }
    return 0;
  }
}

  expect = "TEST";
actual = String.prototype.substr.call(obj, likeZero);

printStatus("Substring length: "+actual.length);
printStatus((expect === actual).toString());

reportCompare(expect, actual, summary);
