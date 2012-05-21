// |reftest| skip
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 446169;
var summary = 'Do not assert: Thin_GetWait(tl->owner) in thread-safe build';
var actual = 'No Crash';
var expect = 'No Crash';

var array = [{}, {}, {}, {}];

function foo() 
{
  for (var i = 0; i != 42*42*42; ++i) 
  {
    var obj = array[i % array.length];
    obj["a"+i] = 1;
    var tmp = {};
    tmp["a"+i] = 2;
  }
}


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  if (typeof scatter == 'function')
  {
    scatter([foo, foo, foo, foo]);
  }
  else
  {
    print('Test skipped. Requires thread-safe build with scatter function.');
  }
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}


