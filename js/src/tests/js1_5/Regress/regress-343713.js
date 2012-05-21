/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 343713;
var summary = 'Do not assert with nested function evaluation';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
with (this)
  with (this) {
  eval("function outer() { function inner() { " +
       "print('inner');} inner(); print('outer');} outer()");
}

reportCompare(expect, actual, summary);
