/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 349012;
var summary = 'generator recursively calling itself via close is an Error';
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

  var iter;
  function gen() {
    iter.close();
    yield 1;
  }

  expect = /TypeError.*[aA]lready executing generator/;
  try
  {
    iter = gen();
    var i = iter.next();
    print("i="+i);
  }
  catch(ex)
  {
    print(ex + '');
    actual = ex + '';
  }
  reportMatch(expect, actual, summary);

  exitFunc ('test');
}
