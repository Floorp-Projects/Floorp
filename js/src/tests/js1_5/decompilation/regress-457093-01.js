/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 457093;
var summary = 'Do not assert: newtop <= oldtop, decompiling function';
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
 
  function Test(object)
  {
    for (var i in object)
    {
      try
      {
      }
      catch(E) {}
    }
  }

  var x = Test.toString();
  
  print(x);

  expect = 'function Test ( object ) { for ( var i in object ) { try { } catch ( E ) { } } }';
  actual = Test + '';

  compareSource(expect, actual, summary);

  exitFunc ('test');
}
