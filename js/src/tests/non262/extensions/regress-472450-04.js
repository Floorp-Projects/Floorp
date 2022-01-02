/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 472450;
var summary = 'TM: Do not assert: StackBase(fp) + blockDepth == regs.sp';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 

  var cyclic = [];
  cyclic[0] = cyclic;
  ({__proto__: cyclic});
  function f(){ 
    eval("for (var y = 0; y < 1; ++y) { for (let z of [null, function(){}, null, '', null, '', null]) { let x = 1, c = []; } }"); 
  }
  f();


  reportCompare(expect, actual, summary);
}
