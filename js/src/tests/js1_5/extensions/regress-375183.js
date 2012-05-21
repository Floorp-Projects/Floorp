/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 375183;
var summary = '__noSuchMethod__ should not allocate beyond fp->script->depth';
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
 
  var obj = { get __noSuchMethod__() {
      print("Executed");
      return new Object();
    }};

  try
  {
    obj.x();
  }
  catch(ex)
  {
  }

  reportCompare(expect, actual, summary + ':1');

  obj = { __noSuchMethod__: {} };
  try
  {
    obj.x();
  }
  catch(ex)
  {
  }

  reportCompare(expect, actual, summary + ':2');

  obj = { }
  obj.__noSuchMethod__ = {};
  try
  {
    obj.x();
  }
  catch(ex)
  {
  }

  reportCompare(expect, actual, summary + ':3');

  exitFunc ('test');
}
