/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 315509;
var summary = 'Array.prototype.unshift do not crash on Arrays with holes';
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
printStatus (summary);

function x1() {
  var a = new Array(1);
  a.unshift(1);
}
function x2() {
  var a = new Array(1);
  a.unshift.call(a, 1);
}
function x3() {
  var a = new Array(1);
  a.x = a.unshift;
  a.x(1);
}
function x4() {
  var a = new Array(1);
  a.__defineSetter__("x", a.unshift);
  a.x = 1;
}

for (var i = 0; i < 10; i++)
{
  x1();
  x2();
  x3();
  x4();
}
 
reportCompare(expect, actual, summary);
