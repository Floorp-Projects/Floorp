/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 367629;
var summary = 'Decompilation of result with native function as getter';
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
 
  var a =
    Object.defineProperty({}, "h",
    {
      get: encodeURI,
      enumerable: true,
      configurable: true
    });

  expect = '({get h() {[native code]}})';
  actual = uneval(a);      

  compareSource(expect, actual, summary);

  exitFunc ('test');
}
