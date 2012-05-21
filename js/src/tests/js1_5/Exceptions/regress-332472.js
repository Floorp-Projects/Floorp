/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 332472;
var summary = 'new RegExp() ignores string boundaries when throwing exceptions';
var actual = '';
var expect = 'SyntaxError: invalid quantifier';

printBugNumber(BUGNUMBER);
printStatus (summary);

var str1 = "?asdf\nAnd you really shouldn't see this!";
var str2 = str1.substr(0, 5);
try {
  new RegExp(str2);
}
catch(ex) {
  printStatus(ex);
  actual = ex + '';
}
 
reportCompare(expect, actual, summary);
