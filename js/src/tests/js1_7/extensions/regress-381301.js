/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 381301;
var summary = 'uneval of object with native-function getter';
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

  var o =
    Object.defineProperty({}, "x", { get: decodeURI, enumerable: true, configurable: true });
  expect = '( { get x ( ) { [ native code ] } } )';
  actual =  uneval(o);

  compareSource(expect, actual, summary);

  exitFunc ('test');
}
