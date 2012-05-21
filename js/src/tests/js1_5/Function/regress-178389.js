/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 178389;
var summary = 'Function.prototype.toSource should not override Function.prototype.toString';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
function f()
{
  var g = function (){};
}

expect = f.toString();

Function.prototype.toSource = function () { return ''; };

actual = f.toString();

reportCompare(expect, actual, summary);
