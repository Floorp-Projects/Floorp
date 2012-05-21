/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 338307;
var summary = 'for (i in arguments) causes type error (JS_1_7_ALPHA_BRANCH)';
var actual = '';
var expect = 'No Error';

printBugNumber(BUGNUMBER);
printStatus (summary);

function f() {
  for (var i in arguments);
}

try
{
  f();
  actual = 'No Error';
}
catch(ex)
{
  actual = ex + '';
}
 
reportCompare(expect, actual, summary);
