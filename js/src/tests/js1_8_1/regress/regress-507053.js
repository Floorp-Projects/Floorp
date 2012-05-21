/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 507053;
var summary = 'TM: invalid results with setting a closure variable in a loop'
var actual = '';
var expect = '2,4,8,16,32,2,4,8,16,32,2,4,8,16,32,2,4,8,16,32,2,4,8,16,32,';


//-----------------------------------------------------------------------------
start_test();
jit(true);

var f = function() {
  var p = 1;
  
  function g() {
    for (var i = 0; i < 5; ++i) {
      p = p * 2;
      actual += p + ',';
    }
  }
  g();
}

for (var i = 0; i < 5; ++i) {
  f();
}

jit(false);
finish_test();
//-----------------------------------------------------------------------------

function start_test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
}

function finish_test()
{
  reportCompare(expect, actual, summary);
  exitFunc ('test');
}
