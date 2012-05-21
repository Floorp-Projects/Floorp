/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 255245;
var summary = 'Function.prototype.toSource/.toString show "setrval" instead of "return"';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

function f() {
  try {
  } catch (e) {
    return false;
  }
  finally {
  }
}

if (typeof f.toSource != 'undefined')
{
  expect = -1;
  actual = f.toSource().indexOf('setrval');

  reportCompare(expect, actual, summary);
}
