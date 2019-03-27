/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 259935;
var summary = 'document.all can be easily detected';
var actual = '';
var expect = 'not detected';

printBugNumber(BUGNUMBER);
printStatus (summary);

if (typeof document == 'undefined')
{
  document = {};
}

function foo() {
  this.ie = document.all;
}

var f = new foo();

if (f.ie) {
  actual = 'detected';
} else {
  actual = 'not detected';
}
 
reportCompare(expect, actual, summary);

f = {ie: document.all};

if (f.ie) {
  actual = 'detected';
} else {
  actual = 'not detected';
}
 
reportCompare(expect, actual, summary);
