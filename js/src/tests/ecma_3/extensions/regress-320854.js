/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 320854;
var summary = 'o.hasOwnProperty("length") should not lie when o has function in proto chain';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var o = {__proto__:function(){}};

expect = false;
actual = o.hasOwnProperty('length')
 
  reportCompare(expect, actual, summary);
