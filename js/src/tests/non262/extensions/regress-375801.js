/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 375801;
var summary = 'uneval should use "(void 0)" instead of "undefined"';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  expect = '({a:(void 0)})'
  actual = uneval({a: undefined})
  compareSource(expect, actual, summary + ': uneval');

  expect = 'function() {({a: undefined});}';
  actual = (function() {({a: undefined});}).toString();
  compareSource(expect, actual, summary + ': toString');

  expect = '(function () {({a: undefined});})';
  actual = (function () {({a: undefined});}).toSource();
  compareSource(expect, actual, summary + ': toSource');
}
