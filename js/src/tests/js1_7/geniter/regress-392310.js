/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 392310;
var summary = 'send on newborn generator';
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

  print("See http://developer.mozilla.org/en/docs/New_in_JavaScript_1.7");

  function yielder() {
    actual = 'before yield';
    yield;
    actual = 'after yield';
  }

  var gen = yielder();
  expect = 'before yield';
  gen.send('foo');
  reportCompare(expect, actual, 'send(value) to newborn generator');

  var gen = yielder();
  expect = 'before yield';
  gen.send(undefined);
  reportCompare(expect, actual, 'send(undefined) to newborn generator');

  exitFunc ('test');
}
