/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465366;
var summary = 'TM: JIT: error with multiplicative loop';
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


  function f()
  {
    var k = 1;
    for (var n = 0; n < 2; n++) {
      k = (k * 10);
    }
    return k;
  }
  f();
  print(f());

 
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
