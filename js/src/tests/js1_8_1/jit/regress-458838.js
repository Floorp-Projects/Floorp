/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 458838;
var summary = 'TM: do not fall off trace when nested function accesses var of outer function';
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

  jit(true);

  function f() {
    var a = 1;
    function g() {
      var b = 0
        for (var i = 0; i < 10; ++i) {
          b += a;
        }
      return b;
    }

    return g();
  }

  expect = 10;
  actual  = f();

  var recorderStarted;
  var recorderAborted;
  var traceCompleted;
  var skip = true;
  
  if (this.tracemonkey && !this.tracemonkey.adaptive)
  {
    recorderStarted = this.tracemonkey.recorderStarted;
    recorderAborted = this.tracemonkey.recorderAborted;
    traceCompleted  = this.tracemonkey.traceCompleted;
    skip = false;
  }

  jit(false);

  reportCompare(expect, actual, summary + ': return value 10');

  if (!skip)
  {
    expect = 'recorderStarted=1, recorderAborted=0, traceCompleted=1';
    actual = 'recorderStarted=' + recorderStarted + ', recorderAborted=' + recorderAborted + ', traceCompleted=' + traceCompleted;
    reportCompare(expect, actual, summary + ': trace');
  }

  exitFunc ('test');
}
