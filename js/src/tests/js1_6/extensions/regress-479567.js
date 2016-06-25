/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 479567;
var summary = 'Do not assert: thing';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);


function f()
{
  (eval("(function(){for each (y in [false, false, false]);});"))();
}

try
{
  this.__defineGetter__("x", gc);
  uneval(this.watch("y", this.toSource))
    f();
  throw x;
}
catch(ex)
{
}


reportCompare(expect, actual, summary);
