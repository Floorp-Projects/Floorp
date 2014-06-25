/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 349012;
var summary = 'generator recursively calling itself via next is an Error';
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

  function gen() {
    var iter = yield;
    try {
      iter.next(1);
    } catch (e) {
      yield e;
    }
  }

  expect = true;
  var iter = gen();
  iter.next();
  var ex = iter.send(iter);
  print(ex + '');
  actual = (ex instanceof TypeError);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
