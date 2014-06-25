/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 469044;
var summary = 'type unstable globals';
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

  expect = '---000---000';
  actual = '';

  for (var i = 0; i < 2; ++i) {
    for (var e = 0; e < 2; ++e) {
    }
    var c = void 0;
    print(actual += "---");
    for (var a = 0; a < 3; ++a) {
      c <<= c;
      print(actual += "" + c);
    }
  }
  reportCompare(expect, actual, summary + ': 1');

  expect = '00000000';
  actual = '';

  print("");
  for (var i = 0; i < 2; ++i) {
    for (var e = 0; e < 2; ++e) {
    }
    var c = void 0;
    for (var a = 0; a < 3; ++a) {
      c <<= c;
      print(actual += "" + c);
    }
    print(actual += c);
  }
  reportCompare(expect, actual, summary + ': 2');

  actual = '';
  print("");

  for (var i = 0; i < 2; ++i) {
    for (var e = 0; e < 2; ++e) {
    }
    var c = void 0;
    for (var a = 0; a < 3; ++a) {
      c <<= c;
      Math;
      print(actual += "" + c);
    }
    print(actual += c);
  }  
  reportCompare(expect, actual, summary + ': 3');

  exitFunc ('test');
}
