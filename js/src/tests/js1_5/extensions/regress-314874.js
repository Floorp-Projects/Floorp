/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 314874;
var summary = 'Function.call/apply with non-primitive argument';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var thisArg = { valueOf: function() { return {a: 'a', b: 'b'}; } };

  var f = function () { return (this); };

  expect  = f.call(thisArg);

  thisArg.f = f;

  actual = thisArg.f();

  delete thisArg.f;

  expect = expect.toSource();
  actual = actual.toSource();
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
