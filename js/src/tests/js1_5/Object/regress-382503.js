/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 382503;
var summary = 'Do not assert: with prototype=regexp';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
function f(x)
{
  prototype = /a/;

  if (x) {
    return /b/;
    return /c/;
  } else {
    return /d/;
  }
}

void f(false);

reportCompare(expect, actual, summary);
