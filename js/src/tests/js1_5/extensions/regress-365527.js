// |reftest| slow
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 365527;
var summary = 'JSOP_ARGUMENTS should set obj register';
var actual = 'No Crash';
var expect = 'No Crash';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 

  counter = 500*1000;

  var obj;

  function getter()
  {
    obj = { get x() {
        return getter();
      }, counter: counter};
    return obj;
  }


  var x;

  function g()
  {
    x += this.counter;
    if (--counter == 0)
      throw "Done";
  }


  function f()
  {
    arguments=g;
    try {
      for (;;) {
	arguments();
	obj.x;
      }
    } catch (e) {
    }
  }


  getter();
  f();

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
