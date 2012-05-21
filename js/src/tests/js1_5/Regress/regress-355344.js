/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 355344;
var summary = 'Exceptions thrown by watch point';
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

  var o = {};

  expect = 'setter: yikes';

  o.watch('x', function(){throw 'yikes'});
  try
  {
    o.x = 3;
  }
  catch(ex)
  {
    actual = "setter: " + ex;
  }

  try
  {
    eval("") ;
  }
  catch(e)
  {
    actual = "eval: " + e;
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
