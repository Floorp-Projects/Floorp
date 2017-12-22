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
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  var o =
    Object.defineProperty({}, "x", { get: decodeURI, enumerable: true, configurable: true });
  expect = '({get x() {[native code]}})';
  actual =  uneval(o);

  // Native function syntax:
  // `function IdentifierName_opt ( FormalParameters ) { [ native code ] }`

  // The placement of whitespace characters in the native function's body is
  // implementation-dependent, so we need to replace those for this test.
  var re = new RegExp(["\\{", "\\[", "native", "code", "\\]", "\\}"].join("\\s*"));
  actual = actual.replace(re, "{[native code]}");

  compareSource(expect, actual, summary);
}
