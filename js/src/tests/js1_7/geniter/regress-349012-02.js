/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 349012;
var summary = 'generators with nested try finally blocks';
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

  expect = "[object StopIteration]";
  var expectyield   = "12";
  var expectfinally = "Inner finally,Outer finally";
  var actualyield = "";
  var actualfinally = "";

  function gen()
  {
    try {
      try {
        yield 1;
      } finally {
        actualfinally += "Inner finally";
        yield 2;
      }
    } finally {
      actualfinally += ",Outer finally";
    }
  }

  var iter = gen();
  actualyield += iter.next();
  actualyield += iter.next();
  try
  {
    actualyield += iter.next();
    actual = "No exception";
  }
  catch(ex)
  {
    actual = ex + '';
  }
 
  reportCompare(expect, actual, summary);
  reportCompare(expectyield, actualyield, summary);
  reportCompare(expectfinally, actualfinally, summary);

  exitFunc ('test');
}
