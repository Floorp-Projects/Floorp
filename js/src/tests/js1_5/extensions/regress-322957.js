/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 322957;
var summary = 'TryMethod should not eat getter exceptions';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var obj = { get toSource() { throw "EXCEPTION"; } };

var got_proper_exception = -1;

try {
  uneval(obj);
} catch (e) {
  got_proper_exception = (e === "EXCEPTION");
}

expect = true;
actual = got_proper_exception;
reportCompare(expect, actual, summary);
