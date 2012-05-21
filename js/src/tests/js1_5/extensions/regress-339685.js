/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 339685;
var summary = 'Setting __proto__ null should not affect __iterator__';
var actual = '';
var expect = 'No Error';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
var d = { a:2, b:3 };

d.__proto__ = null;

try {
  for (var p in d)
    ;
  actual = 'No Error';
} catch(e) {
  actual = e + '';
}

reportCompare(expect, actual, summary);
