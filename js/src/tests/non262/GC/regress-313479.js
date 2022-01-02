/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 313479;
var summary = 'Root access in jsnum.c';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var prepared_string = String(1);
String(2); // To remove prepared_string from newborn

var likeString = {
  toString: function() {
    var tmp = prepared_string;
    prepared_string = null;
    return tmp;
  }
};

var likeNumber = {
  valueOf: function() {
    gc();
    return 10;
  }
}

  var expect = 1;
var actual = parseInt(likeString, likeNumber);
printStatus("expect="+expect+" actual="+actual);
 
reportCompare(expect, actual, summary);
