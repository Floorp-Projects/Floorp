/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 392308;
var summary = 'StopIteration should be catchable';
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

  function testStop() {
    function yielder() {
      actual += 'before, ';
      yield;
      actual += 'after, ';
    }

    expect = 'before, after, iteration terminated normally';

    try {
      var gen = yielder();
      result = gen.next();
      gen.send(result);
    } catch (x if x instanceof StopIteration) {
      actual += 'iteration terminated normally';
    } catch (x2) {
      actual += 'unexpected throw: ' + x2;
    }
  }
  testStop();

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
