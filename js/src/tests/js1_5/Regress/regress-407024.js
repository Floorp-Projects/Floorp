/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 407024;
var summary = 'Do not assert JSVAL_IS_NUMBER(pn3->pn_val) || JSVAL_IS_STRING(pn3->pn_val) || JSVAL_IS_BOOLEAN(pn3->pn_val)';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

eval("function f(x) { switch (x) { case Array: return 1; }}");
var result = f(Array);
if (result !== 1)
  throw "Unexpected result: "+uneval(result);
 
reportCompare(expect, actual, summary);
