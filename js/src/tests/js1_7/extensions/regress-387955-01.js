/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 387955;
var summary = 'Do not Crash [@ TraceEdge]';
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

  var fal = false;

  function gen() { 
    let x;
    function y(){} 
    this.__defineGetter__('', function(){}); 
    if (fal)
      yield;
  }

  for (var i in gen()) { }

  gc();
 
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
