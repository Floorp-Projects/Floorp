// |reftest| skip-if(Android)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 474771;
var summary = 'TM: do not halt execution with gczeal, prototype mangling, for..in';
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

  expect = 'PASS';

  if (typeof gczeal == 'function')
  {
    gczeal(2);
  }

  Object.prototype.q = 3;
  for each (let x in [6, 7]) { } print(actual = "PASS");
 

  delete Object.prototype.q;

  if (typeof gczeal == 'function')
  {
    gczeal(0);
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
