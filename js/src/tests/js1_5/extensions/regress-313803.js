/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 313803;
var summary = 'uneval() on func with embedded objects with getter or setter';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var func = function ff() {
  obj = { get foo() { return "foo"; }};
  return 1;
};

actual = uneval(func);

expect = '(function ff() {obj = {get foo () {return "foo";}};return 1;})';

compareSource(expect, actual, summary);
