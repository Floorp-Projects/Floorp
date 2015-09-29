/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 462734;
var summary = 'Do not assert: pobj_ == obj2';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var obj;
try
{
  Function.prototype.prototype = function() { return 42; }
  obj = Object.create(Function());
  obj.prototype = obj.prototype;
}
catch(ex)
{
}

expect = 'object';
actual = typeof obj.prototype;

reportCompare(expect, actual, summary);
