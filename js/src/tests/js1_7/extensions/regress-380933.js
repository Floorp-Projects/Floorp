/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 380933;
var summary = 'Do not assert with uneval object with setter with modified proto';
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
 
  var f = (function(){}); 
  var y =
    Object.defineProperty({}, "p",
    {
      get: f,
      enumerable: true,
      configurable: true
    });
  f.__proto__ = []; 

  expect = /TypeError: Array.prototype.toSource called on incompatible Function/;
  try
  {
    uneval(y);
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }

  reportMatch(expect, actual, summary);

  exitFunc ('test');
}
