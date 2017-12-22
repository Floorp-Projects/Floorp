/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 245795;
var summary = 'eval(uneval(function)) should be round-trippable';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

function a()
{
  b = function() {};
}

var r = "function a() { b = function() {}; }";
eval(uneval(a));

var v = a.toString().replace(/[ \n]+/g, ' ');
print(v)

printStatus("[" + v + "]");

expect = r;
actual = v;

reportCompare(expect, actual, summary);
